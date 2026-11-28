// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! Top-level async [`Manager`] orchestrating discovery, oneshot reads, and reactive watches.

use std::str::FromStr;
use std::time::Duration;

use tokio::sync::mpsc;

use crate::correlate::correlate;
use crate::error::{Error, Result};
use crate::model::{
    Device, DeviceAddress, DeviceCategory, PowerState, RawObservation, ReadSource, Transport,
    UnavailableReason,
};
use crate::watch::{BatteryWatch, WatchEvent, WatchMode, WatchStrategy};

/// Configuration for [`Manager`].
#[derive(Debug, Clone)]
pub struct ManagerConfig {
    /// Whether to include connected devices whose battery cannot be read as stubs.
    /// Defaults to `true`.
    pub include_stubs: bool,
    /// Whether to actively probe `hidraw` / RFCOMM vendor drivers when standard
    /// OS sources (`sysfs`, `UPower`, `BlueZ`) did not report a battery.
    /// Defaults to `true`.
    pub enable_vendor_probing: bool,
}

impl Default for ManagerConfig {
    fn default() -> Self {
        Self {
            include_stubs: true,
            enable_vendor_probing: true,
        }
    }
}

/// Async entry point for connected device discovery, oneshot battery reads,
/// and reactive [`BatteryWatch`] subscriptions.
#[derive(Clone)]
pub struct Manager {
    config: ManagerConfig,
    #[cfg(any(feature = "upower", feature = "bluez"))]
    dbus: Option<zbus::Connection>,
}

impl Manager {
    /// Creates a new async [`Manager`] with default configuration.
    pub async fn new() -> Result<Self> {
        Self::with_config(ManagerConfig::default()).await
    }

    /// Creates a new async [`Manager`] with explicit [`ManagerConfig`].
    pub async fn with_config(config: ManagerConfig) -> Result<Self> {
        #[cfg(any(feature = "upower", feature = "bluez"))]
        let dbus = zbus::Connection::system().await.ok();

        Ok(Self {
            config,
            #[cfg(any(feature = "upower", feature = "bluez"))]
            dbus,
        })
    }

    /// Performs a oneshot discovery across all enabled standard and vendor providers,
    /// returning deduplicated [`Device`] records (including stubs when `include_stubs` is true).
    pub async fn devices(&self) -> Result<Vec<Device>> {
        #[allow(unused_mut)]
        let mut observations: Vec<RawObservation> = Vec::new();

        // 1. Standardized kernel sysfs power supplies.
        #[cfg(feature = "sysfs")]
        {
            observations.extend(crate::providers::sysfs::discover_power_supplies().await);
        }

        // 2. Standardized UPower D-Bus.
        #[cfg(feature = "upower")]
        if let Some(ref conn) = self.dbus {
            observations.extend(crate::providers::upower::discover(conn).await);
        }

        // 3. Standardized BlueZ (Device1 stubs + Battery1 + multi-cell BLE GATT BAS 0x180F)
        //    plus passive/RFCOMM Bluetooth vendor modules.
        #[cfg(feature = "bluez")]
        if let Some(ref conn) = self.dbus {
            let snapshots = crate::providers::bluez::discover(conn).await;
            let bt_drivers = crate::vendors::bluetooth_drivers();
            for snap in snapshots {
                let needs_vendor = self.config.enable_vendor_probing
                    && (snap.observation.power.is_stub()
                        || snap
                            .observation
                            .power
                            .report()
                            .is_some_and(|r| r.cells.len() == 1));
                let mut obs = snap.observation.clone();

                if needs_vendor {
                    for driver in &bt_drivers {
                        if driver.matches(&snap)
                            && let Some(power) = driver.read_power(&snap).await
                        {
                            // Upgrade if we had a stub or if the vendor driver gave richer multi-cell data.
                            let upgrade = obs.power.is_stub()
                                || power
                                    .report()
                                    .is_some_and(|r| r.cells.len() > 1);
                            if upgrade {
                                obs.power = power;
                                break;
                            }
                        }
                    }
                }
                observations.push(obs);
            }
        }

        // 4. Standard USB HID Power Page (0x84/0x85) + HID vendor quirks, then connected HID
        //    stubs. Vendor observations go first so a paired peripheral's own name (e.g.
        //    "Keychron M6") wins over the receiver's generic product string when merged.
        #[cfg(feature = "sysfs")]
        {
            let hidraw_list = crate::providers::sysfs::enumerate_hidraw_devices().await;

            #[cfg(feature = "hidraw")]
            if self.config.enable_vendor_probing {
                observations.extend(probe_hid_devices(&hidraw_list).await);
            }

            if self.config.include_stubs {
                observations.extend(crate::providers::sysfs::discover_hid_stubs(&hidraw_list).await);
            }
        }

        let mut devices = correlate(observations);
        if !self.config.include_stubs {
            devices.retain(|d| !d.power.is_stub());
        }
        Ok(devices)
    }

    /// Performs a oneshot lookup for a specific connected device by [`DeviceAddress`].
    pub async fn device_by_address(&self, address: &DeviceAddress) -> Result<Option<Device>> {
        let devices = self.devices().await?;
        Ok(devices.into_iter().find(|d| d.matches_address(address)))
    }

    /// Performs a oneshot battery read for a connected device by [`DeviceAddress`].
    ///
    /// Returns `Ok(None)` if no connected device matches `address`, or
    /// `Ok(Some(PowerState::Unavailable { .. }))` if the device is connected as a stub.
    pub async fn power_by_address(&self, address: &DeviceAddress) -> Result<Option<PowerState>> {
        Ok(self.device_by_address(address).await?.map(|d| d.power))
    }

    /// Convenience oneshot battery read parsing `id` as a [`DeviceAddress`]
    /// (e.g. `"AA:BB:CC:DD:EE:FF"`, `"/dev/hidraw3"`, `"/sys/class/power_supply/BAT0"`,
    /// or `"usb:3434:d030"`).
    pub async fn power_by_id(&self, id: &str) -> Result<Option<PowerState>> {
        let addr = DeviceAddress::from_str(id)?;
        self.power_by_address(&addr).await
    }

    /// Starts a reactive [`BatteryWatch`] for the device matching `address` using `mode`.
    ///
    /// - [`WatchMode::Auto`]: uses native D-Bus push events when supported and falls back
    ///   to polling at `poll_interval` only when necessary.
    /// - [`WatchMode::EventOnly`]: uses native push events only; returns
    ///   [`Error::Unsupported`] if the device requires polling.
    /// - [`WatchMode::PollingOnly`]: forces periodic polling at the requested interval.
    ///
    /// Call [`BatteryWatch::is_polling`] or [`BatteryWatch::strategy`] to inspect which
    /// strategy was selected. Dropping the returned [`BatteryWatch`] immediately cancels
    /// all background polling or event listeners.
    pub async fn watch_device(
        &self,
        address: DeviceAddress,
        mode: WatchMode,
    ) -> Result<BatteryWatch> {
        let initial = self.device_by_address(&address).await?;
        let strategy = self.choose_watch_strategy(initial.as_ref(), mode)?;

        let (tx, rx) = mpsc::channel(32);
        let manager = self.clone();

        let join = tokio::spawn(async move {
            let mut last_power: Option<PowerState> = None;

            if let Some(dev) = initial {
                last_power = Some(dev.power.clone());
                let power = dev.power.clone();
                if tx
                    .send(WatchEvent::Updated {
                        device: Box::new(dev),
                        power,
                    })
                    .await
                    .is_err()
                {
                    return;
                }
            }

            match strategy {
                WatchStrategy::EventDriven => {
                    #[cfg(any(feature = "upower", feature = "bluez"))]
                    {
                        if let Err(err) =
                            run_dbus_watch(&manager, &address, &tx, &mut last_power).await
                        {
                            let _ = tx.send(WatchEvent::Error(err)).await;
                        }
                    }
                }
                WatchStrategy::Polling(interval) => {
                    run_polling_watch(&manager, &address, interval, &tx, &mut last_power).await;
                }
            }
        });

        Ok(BatteryWatch::new(rx, join.abort_handle(), strategy))
    }

    /// Convenience wrapper around [`Self::watch_device`] accepting a string address.
    pub async fn watch_id(&self, id: &str, mode: WatchMode) -> Result<BatteryWatch> {
        let addr = DeviceAddress::from_str(id)?;
        self.watch_device(addr, mode).await
    }

    fn choose_watch_strategy(
        &self,
        initial: Option<&Device>,
        mode: WatchMode,
    ) -> Result<WatchStrategy> {
        let supports_events = {
            #[cfg(any(feature = "upower", feature = "bluez"))]
            {
                self.dbus.is_some()
                    && initial
                        .and_then(|dev| dev.power.report())
                        .is_some_and(|report| {
                            matches!(
                                report.source,
                                ReadSource::UPower
                                    | ReadSource::BluezBattery1
                                    | ReadSource::BleGattBas
                            )
                        })
            }
            #[cfg(not(any(feature = "upower", feature = "bluez")))]
            {
                let _ = initial;
                false
            }
        };

        match mode {
            WatchMode::Auto { poll_interval } => {
                if supports_events {
                    Ok(WatchStrategy::EventDriven)
                } else {
                    Ok(WatchStrategy::Polling(
                        poll_interval.max(Duration::from_secs(1)),
                    ))
                }
            }
            WatchMode::EventOnly => {
                if supports_events {
                    Ok(WatchStrategy::EventDriven)
                } else {
                    Err(Error::Unsupported(
                        "device does not support event-driven battery updates",
                    ))
                }
            }
            WatchMode::PollingOnly(interval) => Ok(WatchStrategy::Polling(
                interval.max(Duration::from_secs(1)),
            )),
        }
    }
}

/// Reads standard HID batteries and vendor drivers on every `hidraw` node.
#[cfg(all(feature = "sysfs", feature = "hidraw"))]
async fn probe_hid_devices(
    hidraw_list: &[crate::providers::sysfs::HidrawDeviceInfo],
) -> Vec<RawObservation> {
    let hid_drivers = crate::vendors::hid_drivers();
    let mut observations = Vec::new();

    for info in hidraw_list {
        let dev = crate::providers::hidraw::HidrawDevice::new(&info.dev_path);
        let observation = |power: PowerState, transport: Transport| RawObservation {
            addresses: vec![
                DeviceAddress::HidrawPath(info.dev_path.clone()),
                DeviceAddress::Usb {
                    vendor_id: info.vendor_id,
                    product_id: info.product_id,
                    serial: info.uniq.clone(),
                    bus_addr: None,
                },
            ],
            name: (!info.name.is_empty()).then(|| info.name.clone()),
            vendor: None,
            model: None,
            vendor_id: Some(info.vendor_id),
            product_id: Some(info.product_id),
            category: crate::providers::sysfs::infer_category_from_name(&info.name),
            transport,
            power,
        };

        // First try the standard USB HID Battery System Usage Page (0x85).
        if let Ok(Some(report)) = dev.read_standard_hid_battery().await {
            observations.push(observation(PowerState::Available(report), Transport::Usb));
            continue;
        }

        // Otherwise the first matching proprietary vendor module.
        let Some(driver) = hid_drivers.iter().find(|d| d.matches(info)) else {
            continue;
        };
        let power = driver.read_power(info, &dev).await;
        let permission_denied = matches!(
            power,
            Some(PowerState::Unavailable {
                reason: UnavailableReason::PermissionDenied { .. }
            })
        );
        let identity = if permission_denied {
            None
        } else {
            driver.identify(info, &dev).await
        };
        if power.is_none() && identity.is_none() {
            continue;
        }

        let fallback = match identity {
            Some(ref id) if !id.connected => UnavailableReason::DeviceAsleep,
            _ => UnavailableReason::UnsupportedProtocol,
        };
        let mut obs = observation(
            power.unwrap_or(PowerState::Unavailable { reason: fallback }),
            Transport::WirelessDongle,
        );
        if let Some(id) = identity {
            obs.name = Some(id.name);
            obs.category = id.category;
        }
        observations.push(obs);
    }
    observations
}

async fn run_polling_watch(
    manager: &Manager,
    address: &DeviceAddress,
    interval: Duration,
    tx: &mpsc::Sender<WatchEvent>,
    last_power: &mut Option<PowerState>,
) {
    let mut ticker = tokio::time::interval(interval);
    ticker.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Skip);
    // First tick fires immediately; we already emitted the initial snapshot.
    ticker.tick().await;

    loop {
        ticker.tick().await;
        match manager.device_by_address(address).await {
            Ok(Some(dev)) => {
                if last_power.as_ref() != Some(&dev.power) {
                    *last_power = Some(dev.power.clone());
                    let power = dev.power.clone();
                    if tx
                        .send(WatchEvent::Updated {
                            device: Box::new(dev),
                            power,
                        })
                        .await
                        .is_err()
                    {
                        break;
                    }
                }
            }
            Ok(None) => {
                if last_power.take().is_some()
                    && tx
                        .send(WatchEvent::Disconnected(address.clone()))
                        .await
                        .is_err()
                {
                    break;
                }
            }
            Err(err) => {
                if tx.send(WatchEvent::Error(err)).await.is_err() {
                    break;
                }
            }
        }
    }
}

#[cfg(any(feature = "upower", feature = "bluez"))]
async fn run_dbus_watch(
    manager: &Manager,
    address: &DeviceAddress,
    tx: &mpsc::Sender<WatchEvent>,
    last_power: &mut Option<PowerState>,
) -> Result<()> {
    let Some(ref conn) = manager.dbus else {
        return Ok(());
    };

    let rule = zbus::MatchRule::builder()
        .msg_type(zbus::message::Type::Signal)
        .interface("org.freedesktop.DBus.Properties")?
        .member("PropertiesChanged")?
        .build();

    let mut stream = zbus::MessageStream::for_match_rule(rule, conn, Some(64)).await?;

    while let Some(msg) = tokio::select! {
        _ = tx.closed() => None,
        next = futures_next(&mut stream) => next,
    } {
        let Ok(msg) = msg else { continue };
        let path_str = msg
            .header()
            .path()
            .map(|p| p.as_str().to_string())
            .unwrap_or_default();

        // Only react to UPower or BlueZ property changes.
        if !path_str.starts_with("/org/freedesktop/UPower/")
            && !path_str.starts_with("/org/bluez/")
        {
            continue;
        }

        match manager.device_by_address(address).await {
            Ok(Some(dev)) => {
                if last_power.as_ref() != Some(&dev.power) {
                    *last_power = Some(dev.power.clone());
                    let power = dev.power.clone();
                    if tx
                        .send(WatchEvent::Updated {
                            device: Box::new(dev),
                            power,
                        })
                        .await
                        .is_err()
                    {
                        break;
                    }
                }
            }
            Ok(None) => {
                if last_power.take().is_some()
                    && tx
                        .send(WatchEvent::Disconnected(address.clone()))
                        .await
                        .is_err()
                {
                    break;
                }
            }
            Err(err) => {
                if tx.send(WatchEvent::Error(err)).await.is_err() {
                    break;
                }
            }
        }
    }

    Ok(())
}

#[cfg(any(feature = "upower", feature = "bluez"))]
async fn futures_next(
    stream: &mut zbus::MessageStream,
) -> Option<zbus::Result<zbus::Message>> {
    use zbus::export::ordered_stream::OrderedStreamExt;
    stream.next().await
}

// Keep unused imports clean when features are toggled off.
#[allow(dead_code)]
fn _keep_types_used(_: DeviceCategory, _: Transport, _: UnavailableReason, _: ReadSource) {}

// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! Async `org.freedesktop.UPower` D-Bus provider via `zbus`.

use std::path::PathBuf;

use zbus::{Connection, proxy};

use crate::model::{
    BatteryReport, ChargingState, DeviceAddress, DeviceCategory, PowerState, RawObservation,
    ReadSource, Transport,
};

#[proxy(
    interface = "org.freedesktop.UPower",
    default_service = "org.freedesktop.UPower",
    default_path = "/org/freedesktop/UPower"
)]
trait UPower {
    fn enumerate_devices(&self) -> zbus::Result<Vec<zbus::zvariant::OwnedObjectPath>>;
}

#[proxy(
    interface = "org.freedesktop.UPower.Device",
    default_service = "org.freedesktop.UPower"
)]
trait UPowerDevice {
    #[zbus(property)]
    fn native_path(&self) -> zbus::Result<String>;
    #[zbus(property)]
    fn vendor(&self) -> zbus::Result<String>;
    #[zbus(property)]
    fn model(&self) -> zbus::Result<String>;
    #[zbus(property)]
    fn serial(&self) -> zbus::Result<String>;
    #[zbus(property, name = "Type")]
    fn device_type(&self) -> zbus::Result<u32>;
    #[zbus(property)]
    fn state(&self) -> zbus::Result<u32>;
    #[zbus(property)]
    fn percentage(&self) -> zbus::Result<f64>;
    #[zbus(property)]
    fn is_present(&self) -> zbus::Result<bool>;
}

/// Discovers battery-equipped devices via `org.freedesktop.UPower`.
pub(crate) async fn discover(conn: &Connection) -> Vec<RawObservation> {
    let Ok(proxy) = UPowerProxy::new(conn).await else {
        return Vec::new();
    };
    let Ok(paths) = proxy.enumerate_devices().await else {
        return Vec::new();
    };

    let mut observations = Vec::new();
    for obj_path in paths {
        if let Some(obs) = read_device(conn, obj_path.as_str()).await {
            observations.push(obs);
        }
    }
    observations
}

async fn read_device(conn: &Connection, obj_path: &str) -> Option<RawObservation> {
    let dev = UPowerDeviceProxy::builder(conn)
        .path(obj_path)
        .ok()?
        .build()
        .await
        .ok()?;

    let dev_type = dev.device_type().await.unwrap_or(0);
    // 1 = Line Power (skip non-battery mains)
    if dev_type == 1 {
        return None;
    }

    if !dev.is_present().await.unwrap_or(true) {
        return None;
    }

    let percentage = dev.percentage().await.ok()? as f32;
    let state = dev.state().await.unwrap_or(0);
    let charging = map_upower_state(state);
    let vendor = dev.vendor().await.ok().filter(|s| !s.is_empty());
    let model = dev.model().await.ok().filter(|s| !s.is_empty());
    let serial = dev.serial().await.ok().filter(|s| !s.is_empty());
    let native_path = dev.native_path().await.ok().unwrap_or_default();

    let mut addresses = vec![DeviceAddress::UPowerPath(obj_path.to_string())];
    if let Some(ref ser) = serial
        && let Some(mac) = DeviceAddress::parse_mac(ser)
    {
        addresses.push(DeviceAddress::BluetoothMac(mac));
    } else if let Some(mac) = extract_mac_from_upower_path(obj_path, &native_path) {
        addresses.push(DeviceAddress::BluetoothMac(mac));
    }

    if !native_path.is_empty() && !native_path.starts_with('/') {
        let candidate = PathBuf::from("/sys/class/power_supply").join(&native_path);
        addresses.push(DeviceAddress::SysfsPath(candidate));
    }

    let name = match (&vendor, &model) {
        (Some(v), Some(m)) if !m.starts_with(v.as_str()) => Some(format!("{v} {m}")),
        (_, Some(m)) => Some(m.clone()),
        (Some(v), None) => Some(v.clone()),
        (None, None) => None,
    };

    let transport = if addresses
        .iter()
        .any(|a| matches!(a, DeviceAddress::BluetoothMac(_)))
    {
        Transport::BluetoothClassic
    } else if dev_type == 2 {
        Transport::Internal
    } else {
        Transport::Unknown
    };

    Some(RawObservation {
        addresses,
        name,
        vendor,
        model,
        vendor_id: None,
        product_id: None,
        category: map_upower_type(dev_type),
        transport,
        power: PowerState::Available(BatteryReport::single(
            percentage,
            charging,
            ReadSource::UPower,
        )),
    })
}

fn map_upower_state(state: u32) -> ChargingState {
    match state {
        1 | 5 => ChargingState::Charging,
        2 | 6 => ChargingState::Discharging,
        4 => ChargingState::Full,
        3 => ChargingState::NotCharging,
        _ => ChargingState::Unknown,
    }
}

fn map_upower_type(dev_type: u32) -> DeviceCategory {
    match dev_type {
        1 => DeviceCategory::LinePower,
        2 => DeviceCategory::Laptop,
        3 => DeviceCategory::Ups,
        5 => DeviceCategory::Mouse,
        6 => DeviceCategory::Keyboard,
        8 => DeviceCategory::Phone,
        10 => DeviceCategory::Tablet,
        12 => DeviceCategory::Controller,
        13 => DeviceCategory::Stylus,
        17 => DeviceCategory::Headphones,
        18 => DeviceCategory::Speaker,
        19 => DeviceCategory::Headset,
        24 => DeviceCategory::Wearable,
        _ => DeviceCategory::Unknown,
    }
}

fn extract_mac_from_upower_path(obj_path: &str, native_path: &str) -> Option<[u8; 6]> {
    for candidate in [native_path, obj_path] {
        for segment in candidate.split('/') {
            if let Some(idx) = segment.find("dev_") {
                let slice = &segment[idx..];
                let end = slice.len().min(21); // "dev_AA_BB_CC_DD_EE_FF"
                if let Some(mac) = DeviceAddress::parse_mac(&slice[..end]) {
                    return Some(mac);
                }
            }
            for token in segment.split('-') {
                if let Some(mac) = DeviceAddress::parse_mac(token) {
                    return Some(mac);
                }
            }
        }
    }
    None
}

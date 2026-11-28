// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! Core device, address, and battery telemetry models.
//!
//! These types are OS-independent and designed to represent both devices with
//! rich multi-cell battery telemetry and connected "stub" devices whose power
//! level cannot currently be read.

use std::fmt;
use std::path::PathBuf;
use std::str::FromStr;
use std::time::SystemTime;

#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};

use crate::error::Error;

/// Canonical, stable identifier for a discovered device, suitable as a UI key.
#[derive(Debug, Clone, PartialEq, Eq, Hash, PartialOrd, Ord)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct DeviceId(pub String);

impl fmt::Display for DeviceId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.0)
    }
}

/// A native transport or OS address that can identify or look up a connected device.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum DeviceAddress {
    /// Normalized 6-byte Bluetooth MAC address (Classic BR/EDR or BLE).
    BluetoothMac([u8; 6]),
    /// Linux raw HID node, e.g. `/dev/hidraw3`.
    HidrawPath(PathBuf),
    /// Linux sysfs path (either `/sys/class/power_supply/...` or `/sys/devices/...`).
    SysfsPath(PathBuf),
    /// USB device identity.
    Usb {
        vendor_id: u16,
        product_id: u16,
        serial: Option<String>,
        bus_addr: Option<(u8, u8)>,
    },
    /// UPower D-Bus object path, e.g. `/org/freedesktop/UPower/devices/mouse_...`.
    UPowerPath(String),
}

impl DeviceAddress {
    /// Format a Bluetooth MAC address as uppercase `AA:BB:CC:DD:EE:FF`.
    #[must_use]
    pub fn format_mac(mac: [u8; 6]) -> String {
        format!(
            "{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
        )
    }

    /// Parse a Bluetooth MAC address in `AA:BB:CC:DD:EE:FF`, `AA-BB-CC-DD-EE-FF`,
    /// or BlueZ `dev_AA_BB_CC_DD_EE_FF` form.
    #[must_use]
    pub fn parse_mac(input: &str) -> Option<[u8; 6]> {
        let trimmed = input.trim();
        let body = trimmed.strip_prefix("dev_").unwrap_or(trimmed);
        let sep = if body.contains(':') {
            ':'
        } else if body.contains('-') {
            '-'
        } else if body.contains('_') {
            '_'
        } else {
            return None;
        };

        let mut bytes = [0u8; 6];
        let mut count = 0;
        for part in body.split(sep) {
            if count >= 6 || part.len() != 2 {
                return None;
            }
            bytes[count] = u8::from_str_radix(part, 16).ok()?;
            count += 1;
        }
        (count == 6).then_some(bytes)
    }

    /// Returns a stable canonical string key for this address.
    #[must_use]
    pub fn canonical_key(&self) -> String {
        match self {
            Self::BluetoothMac(mac) => format!("bt:{}", Self::format_mac(*mac)),
            Self::HidrawPath(p) => format!("hidraw:{}", p.display()),
            Self::SysfsPath(p) => format!("sysfs:{}", p.display()),
            Self::Usb {
                vendor_id,
                product_id,
                serial,
                bus_addr,
            } => {
                if let Some(serial) = serial {
                    format!("usb:{vendor_id:04x}:{product_id:04x}:{serial}")
                } else if let Some((bus, addr)) = bus_addr {
                    format!("usb:{vendor_id:04x}:{product_id:04x}@{bus:03}:{addr:03}")
                } else {
                    format!("usb:{vendor_id:04x}:{product_id:04x}")
                }
            }
            Self::UPowerPath(p) => format!("upower:{p}"),
        }
    }

    /// Returns `true` if `self` matches `query` (allowing partial USB queries
    /// where `serial` or `bus_addr` are omitted).
    #[must_use]
    pub fn matches(&self, query: &Self) -> bool {
        match (self, query) {
            (Self::BluetoothMac(a), Self::BluetoothMac(b)) => a == b,
            (Self::HidrawPath(a), Self::HidrawPath(b)) => a == b,
            (Self::SysfsPath(a), Self::SysfsPath(b)) => a == b,
            (Self::UPowerPath(a), Self::UPowerPath(b)) => a == b,
            (
                Self::Usb {
                    vendor_id: v1,
                    product_id: p1,
                    serial: s1,
                    bus_addr: b1,
                },
                Self::Usb {
                    vendor_id: v2,
                    product_id: p2,
                    serial: s2,
                    bus_addr: b2,
                },
            ) => {
                if v1 != v2 || p1 != p2 {
                    return false;
                }
                if let Some(qs) = s2
                    && s1.as_deref() != Some(qs.as_str())
                {
                    return false;
                }
                if let Some(qb) = b2
                    && b1.as_ref() != Some(qb)
                {
                    return false;
                }
                true
            }
            _ => false,
        }
    }
}

impl FromStr for DeviceAddress {
    type Err = Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let trimmed = s.trim();
        if let Some(mac) = Self::parse_mac(trimmed) {
            return Ok(Self::BluetoothMac(mac));
        }
        if trimmed.starts_with("/dev/hidraw") {
            return Ok(Self::HidrawPath(PathBuf::from(trimmed)));
        }
        if trimmed.starts_with("/sys/") {
            return Ok(Self::SysfsPath(PathBuf::from(trimmed)));
        }
        if trimmed.starts_with("/org/freedesktop/UPower/") {
            return Ok(Self::UPowerPath(trimmed.to_string()));
        }
        if let Some(rest) = trimmed.strip_prefix("usb:")
            && let Some((vid_str, pid_str)) = rest.split_once(':')
            && let (Ok(vid), Ok(pid)) = (
                u16::from_str_radix(vid_str, 16),
                u16::from_str_radix(pid_str, 16),
            )
        {
            return Ok(Self::Usb {
                vendor_id: vid,
                product_id: pid,
                serial: None,
                bus_addr: None,
            });
        }
        Err(Error::InvalidAddress(trimmed.to_string()))
    }
}

/// High-level device category for UI icons and grouping.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Default)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum DeviceCategory {
    Laptop,
    LinePower,
    Ups,
    Mouse,
    Keyboard,
    Headset,
    Earbuds,
    Headphones,
    Speaker,
    Controller,
    Tablet,
    Phone,
    Wearable,
    Stylus,
    #[default]
    Unknown,
}

/// Primary transport over which the device is connected.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Default)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum Transport {
    Internal,
    Usb,
    WirelessDongle,
    BluetoothClassic,
    Ble,
    #[default]
    Unknown,
}

/// Role of an individual battery cell in a single- or multi-cell device.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Default)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum CellRole {
    #[default]
    Primary,
    Left,
    Right,
    Case,
    Dock,
    Spare,
}

/// Coarse battery bucket for hardware that only reports discrete bars instead of a percentage.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum CoarseLevel {
    Critical,
    Low,
    Normal,
    High,
    Full,
}

/// Charging state of a battery cell.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Default)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum ChargingState {
    Charging,
    Discharging,
    Full,
    NotCharging,
    #[default]
    Unknown,
}

/// Which protocol or OS interface produced a [`BatteryReport`].
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum ReadSource {
    /// Linux `/sys/class/power_supply/*`.
    Sysfs,
    /// `org.freedesktop.UPower` D-Bus service.
    UPower,
    /// BlueZ `org.bluez.Battery1` (GATT BAS or HFP via PipeWire/WirePlumber).
    BluezBattery1,
    /// Direct BLE GATT Battery Service (`0x180F` / `0x2A19`) read.
    BleGattBas,
    /// Standard USB HID Power Device / Battery System (`Usage Page 0x84`/`0x85`).
    HidPowerPage,
    /// Vendor-specific proprietary protocol driver.
    Vendor(String),
}

impl ReadSource {
    /// Priority used when merging multiple battery readings for the same physical device.
    /// Higher values win (multi-cell readings receive an additional bonus during correlation).
    #[must_use]
    pub fn priority(&self) -> u8 {
        match self {
            Self::BleGattBas => 60,
            Self::Sysfs => 50,
            Self::UPower => 40,
            Self::BluezBattery1 => 35,
            Self::HidPowerPage => 30,
            Self::Vendor(_) => 25,
        }
    }
}

/// Telemetry for a single battery cell.
#[derive(Debug, Clone, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct BatteryCell {
    pub role: CellRole,
    /// Charge level in `0.0..=100.0`, when known.
    pub percentage: Option<f32>,
    /// Coarse level when hardware only reports discrete steps.
    pub coarse: Option<CoarseLevel>,
    pub charging: ChargingState,
    /// Optional cell voltage in millivolts.
    pub voltage_mv: Option<u32>,
}

impl BatteryCell {
    /// Convenience constructor for a single primary percentage cell.
    #[must_use]
    pub fn primary(percentage: f32, charging: ChargingState) -> Self {
        Self {
            role: CellRole::Primary,
            percentage: Some(percentage.clamp(0.0, 100.0)),
            coarse: None,
            charging,
            voltage_mv: None,
        }
    }
}

/// A battery reading for a device, supporting both single-cell and multi-cell hardware.
#[derive(Debug, Clone, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct BatteryReport {
    pub cells: Vec<BatteryCell>,
    pub source: ReadSource,
    pub updated_at: SystemTime,
}

impl BatteryReport {
    /// Convenience constructor for a single-cell battery report.
    #[must_use]
    pub fn single(percentage: f32, charging: ChargingState, source: ReadSource) -> Self {
        Self {
            cells: vec![BatteryCell::primary(percentage, charging)],
            source,
            updated_at: SystemTime::now(),
        }
    }

    /// Returns the primary cell's percentage, or the first cell with a known percentage.
    #[must_use]
    pub fn primary_percentage(&self) -> Option<f32> {
        self.cells
            .iter()
            .find(|c| c.role == CellRole::Primary)
            .and_then(|c| c.percentage)
            .or_else(|| self.cells.iter().find_map(|c| c.percentage))
    }

    /// Returns a composite quality score used when choosing between multiple reports
    /// for the same device.
    #[must_use]
    pub fn quality_score(&self) -> u16 {
        let base = u16::from(self.source.priority());
        let multi_cell_bonus = if self.cells.len() > 1 { 40 } else { 0 };
        let exact_pct_bonus = if self.cells.iter().any(|c| c.percentage.is_some()) {
            10
        } else {
            0
        };
        base + multi_cell_bonus + exact_pct_bonus
    }
}

/// Why a connected device does not have a readable battery level.
#[derive(Debug, Clone, PartialEq, Eq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum UnavailableReason {
    /// Device is mains-powered or wired without a battery.
    NoBattery,
    /// Connected, but neither standard interfaces nor enabled vendor drivers can read it.
    UnsupportedProtocol,
    /// A raw HID / USB driver matched, but the OS denied access (missing `udev` `uaccess` rule).
    PermissionDenied { path: PathBuf },
    /// Required OS stack feature is disabled (e.g. BlueZ `Experimental = true`).
    StackFeatureDisabled(String),
    /// Wireless dongle is connected, but the paired peripheral is asleep or out of range.
    DeviceAsleep,
    /// Transient I/O, D-Bus, or protocol error.
    ReadError(String),
}

/// Power status of a discovered device.
#[derive(Debug, Clone, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum PowerState {
    /// Battery telemetry was successfully read.
    Available(BatteryReport),
    /// Device is connected and tracked, but its battery could not be read (UI stub).
    Unavailable { reason: UnavailableReason },
}

impl PowerState {
    /// Returns the [`BatteryReport`] if available.
    #[must_use]
    pub fn report(&self) -> Option<&BatteryReport> {
        match self {
            Self::Available(r) => Some(r),
            Self::Unavailable { .. } => None,
        }
    }

    /// Returns `true` if this device is a connected stub with no readable battery.
    #[must_use]
    pub fn is_stub(&self) -> bool {
        matches!(self, Self::Unavailable { .. })
    }
}

/// A connected device discovered by `scintilla`, with either live battery telemetry
/// or a structured [`PowerState::Unavailable`] stub.
#[derive(Debug, Clone, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct Device {
    pub id: DeviceId,
    pub addresses: Vec<DeviceAddress>,
    pub name: String,
    pub vendor: Option<String>,
    pub model: Option<String>,
    pub vendor_id: Option<u16>,
    pub product_id: Option<u16>,
    pub category: DeviceCategory,
    pub transport: Transport,
    pub power: PowerState,
}

impl Device {
    /// Returns `true` if any of this device's addresses match `query`.
    #[must_use]
    pub fn matches_address(&self, query: &DeviceAddress) -> bool {
        self.addresses.iter().any(|addr| addr.matches(query))
    }
}

/// Change event emitted by [`crate::Manager::watch`].
#[derive(Debug, Clone, PartialEq)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub enum DeviceEvent {
    Added(Device),
    Updated(Device),
    Removed(DeviceId),
}

/// Raw observation produced by a single provider before correlation/deduplication.
#[derive(Debug, Clone)]
pub(crate) struct RawObservation {
    pub addresses: Vec<DeviceAddress>,
    pub name: Option<String>,
    pub vendor: Option<String>,
    pub model: Option<String>,
    pub vendor_id: Option<u16>,
    pub product_id: Option<u16>,
    pub category: DeviceCategory,
    pub transport: Transport,
    pub power: PowerState,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_mac_variants() {
        let expected = Some([0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF]);
        assert_eq!(DeviceAddress::parse_mac("aa:bb:cc:dd:ee:ff"), expected);
        assert_eq!(DeviceAddress::parse_mac("AA-BB-CC-DD-EE-FF"), expected);
        assert_eq!(DeviceAddress::parse_mac("dev_AA_BB_CC_DD_EE_FF"), expected);
        assert_eq!(DeviceAddress::parse_mac("not-a-mac"), None);
    }

    #[test]
    fn parses_address_strings() {
        assert!(matches!(
            "AA:BB:CC:DD:EE:FF".parse::<DeviceAddress>(),
            Ok(DeviceAddress::BluetoothMac(_))
        ));
        assert!(matches!(
            "/dev/hidraw2".parse::<DeviceAddress>(),
            Ok(DeviceAddress::HidrawPath(_))
        ));
        assert!(matches!(
            "/sys/class/power_supply/BAT0".parse::<DeviceAddress>(),
            Ok(DeviceAddress::SysfsPath(_))
        ));
        assert!(matches!(
            "usb:3434:d030".parse::<DeviceAddress>(),
            Ok(DeviceAddress::Usb {
                vendor_id: 0x3434,
                product_id: 0xd030,
                ..
            })
        ));
    }
}

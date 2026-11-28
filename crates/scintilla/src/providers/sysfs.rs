// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! Async Linux `sysfs` provider for `/sys/class/power_supply/*` and USB/HID stub discovery.

use std::path::{Path, PathBuf};

use tokio::fs;

use crate::model::{
    BatteryCell, BatteryReport, CellRole, ChargingState, CoarseLevel, DeviceAddress,
    DeviceCategory, PowerState, RawObservation, ReadSource, Transport, UnavailableReason,
};

/// Metadata discovered for a `/dev/hidraw*` node from `sysfs`.
#[derive(Debug, Clone)]
pub struct HidrawDeviceInfo {
    pub dev_path: PathBuf,
    pub sysfs_path: PathBuf,
    pub name: String,
    pub vendor_id: u16,
    pub product_id: u16,
    pub bus_type: u16,
    pub uniq: Option<String>,
    pub phys: Option<String>,
    pub interface_number: Option<u8>,
}

/// Discovers power supplies from `/sys/class/power_supply/*`.
pub(crate) async fn discover_power_supplies() -> Vec<RawObservation> {
    let root = Path::new("/sys/class/power_supply");
    let mut entries = match fs::read_dir(root).await {
        Ok(rd) => rd,
        Err(_) => return Vec::new(),
    };

    let mut observations = Vec::new();
    while let Ok(Some(entry)) = entries.next_entry().await {
        let path = entry.path();
        if let Some(obs) = read_power_supply(&path).await {
            observations.push(obs);
        }
    }
    observations
}

/// Reads a single `/sys/class/power_supply/<name>` entry.
pub(crate) async fn read_power_supply(path: &Path) -> Option<RawObservation> {
    let dir_name = path.file_name()?.to_string_lossy().to_string();
    let supply_type = read_trimmed(path.join("type")).await.unwrap_or_default();

    // Skip mains/AC adapters unless we want them; focus on batteries & UPSes.
    if supply_type.eq_ignore_ascii_case("Mains") {
        return None;
    }

    let present = read_trimmed(path.join("present"))
        .await
        .map(|v| v != "0")
        .unwrap_or(true);
    if !present {
        return None;
    }

    let capacity = read_trimmed(path.join("capacity"))
        .await
        .and_then(|s| s.parse::<f32>().ok());
    let coarse = read_trimmed(path.join("capacity_level"))
        .await
        .as_deref()
        .and_then(parse_capacity_level);
    let status = read_trimmed(path.join("status"))
        .await
        .as_deref()
        .map(parse_charging_status)
        .unwrap_or(ChargingState::Unknown);
    let voltage_mv = read_trimmed(path.join("voltage_now"))
        .await
        .and_then(|s| s.parse::<u64>().ok())
        .map(|uv| (uv / 1000) as u32);

    let model = read_trimmed(path.join("model_name")).await;
    let vendor = read_trimmed(path.join("manufacturer")).await;
    let scope = read_trimmed(path.join("scope")).await.unwrap_or_default();

    let mut addresses = vec![DeviceAddress::SysfsPath(path.to_path_buf())];

    // Kernel HID power supplies often encode the Bluetooth MAC in the directory name
    // (e.g. `hid-aa:bb:cc:dd:ee:ff-battery`) or in `serial_number`.
    if let Some(mac) = extract_mac_from_str(&dir_name) {
        addresses.push(DeviceAddress::BluetoothMac(mac));
    } else if let Some(serial) = read_trimmed(path.join("serial_number")).await
        && let Some(mac) = DeviceAddress::parse_mac(&serial)
    {
        addresses.push(DeviceAddress::BluetoothMac(mac));
    }

    let transport = if dir_name.starts_with("hid-") || dir_name.contains("bluetooth") {
        Transport::BluetoothClassic
    } else if scope.eq_ignore_ascii_case("Device") {
        Transport::WirelessDongle
    } else {
        Transport::Internal
    };

    let category = if supply_type.eq_ignore_ascii_case("UPS") {
        DeviceCategory::Ups
    } else if scope.eq_ignore_ascii_case("System") || dir_name.starts_with("BAT") {
        DeviceCategory::Laptop
    } else {
        infer_category_from_name(model.as_deref().unwrap_or(&dir_name))
    };

    let power = if capacity.is_some() || coarse.is_some() {
        PowerState::Available(BatteryReport {
            cells: vec![BatteryCell {
                role: CellRole::Primary,
                percentage: capacity.map(|c| c.clamp(0.0, 100.0)),
                coarse,
                charging: status,
                voltage_mv,
            }],
            source: ReadSource::Sysfs,
            updated_at: std::time::SystemTime::now(),
        })
    } else {
        PowerState::Unavailable {
            reason: UnavailableReason::ReadError("power_supply has no capacity attribute".into()),
        }
    };

    Some(RawObservation {
        addresses,
        name: model.clone().or(Some(dir_name)),
        vendor,
        model,
        vendor_id: None,
        product_id: None,
        category,
        transport,
        power,
    })
}

/// Enumerates all `/sys/class/hidraw/hidraw*` nodes asynchronously.
pub(crate) async fn enumerate_hidraw_devices() -> Vec<HidrawDeviceInfo> {
    let root = Path::new("/sys/class/hidraw");
    let mut entries = match fs::read_dir(root).await {
        Ok(rd) => rd,
        Err(_) => return Vec::new(),
    };

    let mut devices = Vec::new();
    while let Ok(Some(entry)) = entries.next_entry().await {
        let node_name = entry.file_name().to_string_lossy().to_string();
        if !node_name.starts_with("hidraw") {
            continue;
        }
        let sysfs_path = entry.path();
        let uevent_path = sysfs_path.join("device/uevent");
        let Ok(uevent) = fs::read_to_string(&uevent_path).await else {
            continue;
        };

        let mut name = String::new();
        let mut bus_type = 0u16;
        let mut vendor_id = 0u16;
        let mut product_id = 0u16;
        let mut uniq = None;
        let mut phys = None;

        for line in uevent.lines() {
            if let Some(val) = line.strip_prefix("HID_NAME=") {
                name = val.trim().to_string();
            } else if let Some(val) = line.strip_prefix("HID_ID=") {
                let parts: Vec<&str> = val.trim().split(':').collect();
                if parts.len() == 3 {
                    bus_type = u16::from_str_radix(parts[0], 16).unwrap_or(0);
                    vendor_id = u16::from_str_radix(parts[1], 16).unwrap_or(0);
                    product_id = u16::from_str_radix(parts[2], 16).unwrap_or(0);
                }
            } else if let Some(val) = line.strip_prefix("HID_UNIQ=") {
                let v = val.trim();
                if !v.is_empty() {
                    uniq = Some(v.to_string());
                }
            } else if let Some(val) = line.strip_prefix("HID_PHYS=") {
                let v = val.trim();
                if !v.is_empty() {
                    phys = Some(v.to_string());
                }
            }
        }

        // Try reading USB interface number from parent interface uevent/bInterfaceNumber.
        let interface_number = read_trimmed(sysfs_path.join("device/../bInterfaceNumber"))
            .await
            .and_then(|s| u8::from_str_radix(s.trim(), 16).ok());

        devices.push(HidrawDeviceInfo {
            dev_path: PathBuf::from(format!("/dev/{node_name}")),
            sysfs_path,
            name,
            vendor_id,
            product_id,
            bus_type,
            uniq,
            phys,
            interface_number,
        });
    }
    devices
}

/// Discovers connected HID devices as baseline stubs so devices without readable
/// batteries are still surfaced to consumers.
pub(crate) async fn discover_hid_stubs(hidraw_devices: &[HidrawDeviceInfo]) -> Vec<RawObservation> {
    let mut observations = Vec::new();
    for info in hidraw_devices {
        if info.vendor_id == 0 && info.product_id == 0 {
            continue;
        }

        let mut addresses = vec![
            DeviceAddress::HidrawPath(info.dev_path.clone()),
            DeviceAddress::Usb {
                vendor_id: info.vendor_id,
                product_id: info.product_id,
                serial: info.uniq.clone(),
                bus_addr: None,
            },
        ];

        // Bus 0x0005 is BUS_BLUETOOTH in Linux <linux/input.h>.
        let transport = if info.bus_type == 0x0005 {
            if let Some(ref uniq) = info.uniq
                && let Some(mac) = DeviceAddress::parse_mac(uniq)
            {
                addresses.push(DeviceAddress::BluetoothMac(mac));
            }
            Transport::BluetoothClassic
        } else {
            Transport::Usb
        };

        let category = infer_category_from_name(&info.name);
        observations.push(RawObservation {
            addresses,
            name: (!info.name.is_empty()).then(|| info.name.clone()),
            vendor: None,
            model: None,
            vendor_id: Some(info.vendor_id),
            product_id: Some(info.product_id),
            category,
            transport,
            power: PowerState::Unavailable {
                reason: UnavailableReason::UnsupportedProtocol,
            },
        });
    }
    observations
}

async fn read_trimmed(path: PathBuf) -> Option<String> {
    fs::read_to_string(path)
        .await
        .ok()
        .map(|s| s.trim().to_string())
        .filter(|s| !s.is_empty())
}

fn parse_charging_status(s: &str) -> ChargingState {
    match s.trim().to_ascii_lowercase().as_str() {
        "charging" => ChargingState::Charging,
        "discharging" => ChargingState::Discharging,
        "full" => ChargingState::Full,
        "not charging" => ChargingState::NotCharging,
        _ => ChargingState::Unknown,
    }
}

fn parse_capacity_level(s: &str) -> Option<CoarseLevel> {
    match s.trim().to_ascii_lowercase().as_str() {
        "critical" => Some(CoarseLevel::Critical),
        "low" => Some(CoarseLevel::Low),
        "normal" => Some(CoarseLevel::Normal),
        "high" => Some(CoarseLevel::High),
        "full" => Some(CoarseLevel::Full),
        _ => None,
    }
}

fn extract_mac_from_str(s: &str) -> Option<[u8; 6]> {
    // Handles patterns like `hid-aa:bb:cc:dd:ee:ff-battery` or `ps-controller-battery-aa:bb:cc:dd:ee:ff`
    for token in s.split('-') {
        if let Some(mac) = DeviceAddress::parse_mac(token) {
            return Some(mac);
        }
    }
    None
}

/// Best-effort category heuristic from a device's product string.
#[must_use]
pub(crate) fn infer_category_from_name(name: &str) -> DeviceCategory {
    let lower = name.to_ascii_lowercase();
    if lower.contains("mouse")
        || lower.contains("viper")
        || lower.contains("deathadder")
        || lower.contains("basilisk")
        || lower.contains("aerox")
        || lower.contains("mx master")
        || lower.contains("keychron m")
    {
        DeviceCategory::Mouse
    } else if lower.contains("keyboard") || lower.contains("keychron k") || lower.contains("corne")
    {
        DeviceCategory::Keyboard
    } else if lower.contains("airpods") || lower.contains("buds") || lower.contains("wf-1000") {
        DeviceCategory::Earbuds
    } else if lower.contains("headset")
        || lower.contains("arctis")
        || lower.contains("virtuoso")
        || lower.contains("void")
        || lower.contains("cloud ")
    {
        DeviceCategory::Headset
    } else if lower.contains("wh-1000")
        || lower.contains("quietcomfort")
        || lower.contains("headphone")
    {
        DeviceCategory::Headphones
    } else if lower.contains("dualsense")
        || lower.contains("dualshock")
        || lower.contains("xbox")
        || lower.contains("controller")
        || lower.contains("gamepad")
    {
        DeviceCategory::Controller
    } else if lower.contains("wacom") || lower.contains("tablet") {
        DeviceCategory::Tablet
    } else if lower.contains("ups") {
        DeviceCategory::Ups
    } else {
        DeviceCategory::Unknown
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn extracts_mac_from_sysfs_hid_name() {
        assert_eq!(
            extract_mac_from_str("hid-aa:bb:cc:dd:ee:ff-battery"),
            Some([0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF])
        );
        assert_eq!(
            extract_mac_from_str("ps-controller-battery-11:22:33:44:55:66"),
            Some([0x11, 0x22, 0x33, 0x44, 0x55, 0x66])
        );
    }

    #[test]
    fn infers_categories() {
        assert_eq!(infer_category_from_name("Keychron M6"), DeviceCategory::Mouse);
        assert_eq!(infer_category_from_name("AirPods Pro"), DeviceCategory::Earbuds);
        assert_eq!(infer_category_from_name("SteelSeries Arctis Nova 7"), DeviceCategory::Headset);
        assert_eq!(infer_category_from_name("Sony WH-1000XM5"), DeviceCategory::Headphones);
    }
}

// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! Identity correlation and deduplication across discovery and battery providers.
//!
//! A single physical peripheral (for example a wireless mouse) frequently shows
//! up simultaneously in `sysfs`, `UPower`, `BlueZ`, and `/dev/hidraw*`. This
//! module merges observations sharing any strong hardware address and selects
//! the highest-quality [`PowerState`].

use crate::model::{
    Device, DeviceAddress, DeviceCategory, DeviceId, PowerState, RawObservation, Transport,
    UnavailableReason,
};

/// Correlates and deduplicates raw provider observations into canonical [`Device`] records.
#[must_use]
pub(crate) fn correlate(observations: Vec<RawObservation>) -> Vec<Device> {
    let mut groups: Vec<RawObservation> = Vec::new();

    for obs in observations {
        if let Some(existing) = groups
            .iter_mut()
            .find(|g| share_identity(&g.addresses, &obs.addresses))
        {
            merge_into(existing, obs);
        } else {
            groups.push(obs);
        }
    }

    // Second pass: transitive merges (e.g. A linked to B via MAC, B linked to C via hidraw).
    let mut i = 0;
    while i < groups.len() {
        let mut j = i + 1;
        while j < groups.len() {
            if share_identity(&groups[i].addresses, &groups[j].addresses) {
                let merged = groups.remove(j);
                merge_into(&mut groups[i], merged);
            } else {
                j += 1;
            }
        }
        i += 1;
    }

    let mut devices: Vec<Device> = groups.into_iter().map(finalize).collect();
    devices.sort_by(|a, b| a.id.cmp(&b.id));
    devices
}

fn share_identity(a: &[DeviceAddress], b: &[DeviceAddress]) -> bool {
    for addr_a in a {
        for addr_b in b {
            if addresses_link(addr_a, addr_b) {
                return true;
            }
        }
    }
    false
}

fn addresses_link(a: &DeviceAddress, b: &DeviceAddress) -> bool {
    match (a, b) {
        (DeviceAddress::BluetoothMac(x), DeviceAddress::BluetoothMac(y)) => x == y,
        (DeviceAddress::HidrawPath(x), DeviceAddress::HidrawPath(y)) => x == y,
        (DeviceAddress::SysfsPath(x), DeviceAddress::SysfsPath(y)) => x == y,
        (DeviceAddress::UPowerPath(x), DeviceAddress::UPowerPath(y)) => x == y,
        (
            DeviceAddress::Usb {
                vendor_id: v1,
                product_id: p1,
                serial: s1,
                bus_addr: b1,
            },
            DeviceAddress::Usb {
                vendor_id: v2,
                product_id: p2,
                serial: s2,
                bus_addr: b2,
            },
        ) => {
            if v1 != v2 || p1 != p2 {
                return false;
            }
            if let (Some(sa), Some(sb)) = (s1, s2) {
                return sa == sb;
            }
            if let (Some(ba), Some(bb)) = (b1, b2) {
                return ba == bb;
            }
            // Same VID:PID without distinguishing serial or bus address: link only if
            // both are generic matches (avoids falsely merging two identical mice on
            // different USB ports when bus_addr is known).
            s1.is_none() && s2.is_none() && b1.is_none() && b2.is_none()
        }
        _ => false,
    }
}

fn merge_into(target: &mut RawObservation, incoming: RawObservation) {
    for addr in incoming.addresses {
        if !target.addresses.contains(&addr) {
            target.addresses.push(addr);
        }
    }

    if prefer_name(target.name.as_deref(), incoming.name.as_deref()) {
        target.name = incoming.name;
    }
    if target.vendor.is_none() {
        target.vendor = incoming.vendor;
    }
    if target.model.is_none() {
        target.model = incoming.model;
    }
    if target.vendor_id.is_none() {
        target.vendor_id = incoming.vendor_id;
    }
    if target.product_id.is_none() {
        target.product_id = incoming.product_id;
    }
    if target.category == DeviceCategory::Unknown {
        target.category = incoming.category;
    }
    if target.transport == Transport::Unknown {
        target.transport = incoming.transport;
    }

    if prefer_power(&target.power, &incoming.power) {
        target.power = incoming.power;
    }
}

fn prefer_name(current: Option<&str>, candidate: Option<&str>) -> bool {
    match (current, candidate) {
        (_, None) => false,
        (None, Some(_)) => true,
        (Some(cur), Some(cand)) => {
            let cur_generic = cur.is_empty() || cur.starts_with("hid-") || cur.starts_with("BAT");
            let cand_generic =
                cand.is_empty() || cand.starts_with("hid-") || cand.starts_with("BAT");
            // A longer name only wins when it is a more specific form of the current one
            // ("Pixel" → "Pixel 9"); otherwise the first, most specific source keeps its name
            // (a paired mouse's "Keychron M6" over its receiver's "Keychron Link-KM").
            (cur_generic && !cand_generic)
                || (!cand_generic && cand.len() > cur.len() && cand.contains(cur))
        }
    }
}

fn prefer_power(current: &PowerState, incoming: &PowerState) -> bool {
    match (current, incoming) {
        (PowerState::Available(cur), PowerState::Available(inc)) => {
            inc.quality_score() > cur.quality_score()
        }
        (PowerState::Unavailable { .. }, PowerState::Available(_)) => true,
        (PowerState::Available(_), PowerState::Unavailable { .. }) => false,
        (
            PowerState::Unavailable { reason: r_cur },
            PowerState::Unavailable { reason: r_inc },
        ) => unavailable_specificity(r_inc) > unavailable_specificity(r_cur),
    }
}

fn unavailable_specificity(reason: &UnavailableReason) -> u8 {
    match reason {
        UnavailableReason::PermissionDenied { .. } => 50,
        UnavailableReason::DeviceAsleep => 40,
        UnavailableReason::StackFeatureDisabled(_) => 35,
        UnavailableReason::ReadError(_) => 25,
        UnavailableReason::NoBattery => 20,
        UnavailableReason::UnsupportedProtocol => 10,
    }
}

fn finalize(obs: RawObservation) -> Device {
    let id = choose_canonical_id(&obs.addresses, obs.name.as_deref());
    let name = obs
        .name
        .filter(|n| !n.trim().is_empty())
        .unwrap_or_else(|| id.0.clone());

    Device {
        id,
        addresses: obs.addresses,
        name,
        vendor: obs.vendor,
        model: obs.model,
        vendor_id: obs.vendor_id,
        product_id: obs.product_id,
        category: obs.category,
        transport: obs.transport,
        power: obs.power,
    }
}

fn choose_canonical_id(addresses: &[DeviceAddress], fallback_name: Option<&str>) -> DeviceId {
    // Prefer Bluetooth MAC > USB > Sysfs > UPower > Hidraw
    let best = addresses.iter().min_by_key(|a| match a {
        DeviceAddress::BluetoothMac(_) => 0,
        DeviceAddress::Usb { .. } => 1,
        DeviceAddress::SysfsPath(_) => 2,
        DeviceAddress::UPowerPath(_) => 3,
        DeviceAddress::HidrawPath(_) => 4,
    });

    match best {
        Some(addr) => DeviceId(addr.canonical_key()),
        None => DeviceId(format!("unknown:{}", fallback_name.unwrap_or("device"))),
    }
}

#[cfg(test)]
mod tests {
    use std::path::PathBuf;

    use super::*;
    use crate::model::{BatteryCell, BatteryReport, CellRole, ChargingState, ReadSource};

    #[test]
    fn merges_bluez_stub_with_sysfs_battery_by_mac() {
        let mac = [0x11, 0x22, 0x33, 0x44, 0x55, 0x66];
        let stub = RawObservation {
            addresses: vec![DeviceAddress::BluetoothMac(mac)],
            name: Some("Keychron M6".to_string()),
            vendor: Some("Keychron".to_string()),
            model: None,
            vendor_id: None,
            product_id: None,
            category: DeviceCategory::Mouse,
            transport: Transport::Ble,
            power: PowerState::Unavailable {
                reason: UnavailableReason::UnsupportedProtocol,
            },
        };
        let sysfs = RawObservation {
            addresses: vec![
                DeviceAddress::BluetoothMac(mac),
                DeviceAddress::SysfsPath(PathBuf::from(
                    "/sys/class/power_supply/hid-11:22:33:44:55:66-battery",
                )),
            ],
            name: Some("hid-11:22:33:44:55:66-battery".to_string()),
            vendor: None,
            model: None,
            vendor_id: None,
            product_id: None,
            category: DeviceCategory::Unknown,
            transport: Transport::Unknown,
            power: PowerState::Available(BatteryReport::single(
                82.0,
                ChargingState::Discharging,
                ReadSource::Sysfs,
            )),
        };

        let result = correlate(vec![stub, sysfs]);
        assert_eq!(result.len(), 1);
        assert_eq!(result[0].name, "Keychron M6");
        assert_eq!(result[0].category, DeviceCategory::Mouse);
        assert_eq!(
            result[0].power.report().and_then(|r| r.primary_percentage()),
            Some(82.0)
        );
        assert_eq!(result[0].addresses.len(), 2);
    }

    #[test]
    fn prefers_multi_cell_ble_gatt_over_single_cell_upower() {
        let mac = [0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF];
        let upower = RawObservation {
            addresses: vec![DeviceAddress::BluetoothMac(mac)],
            name: Some("Corne Split".to_string()),
            vendor: None,
            model: None,
            vendor_id: None,
            product_id: None,
            category: DeviceCategory::Keyboard,
            transport: Transport::Ble,
            power: PowerState::Available(BatteryReport::single(
                75.0,
                ChargingState::Discharging,
                ReadSource::UPower,
            )),
        };
        let gatt = RawObservation {
            addresses: vec![DeviceAddress::BluetoothMac(mac)],
            name: None,
            vendor: None,
            model: None,
            vendor_id: None,
            product_id: None,
            category: DeviceCategory::Unknown,
            transport: Transport::Ble,
            power: PowerState::Available(BatteryReport {
                cells: vec![
                    BatteryCell {
                        role: CellRole::Left,
                        percentage: Some(75.0),
                        coarse: None,
                        charging: ChargingState::Discharging,
                        voltage_mv: None,
                    },
                    BatteryCell {
                        role: CellRole::Right,
                        percentage: Some(68.0),
                        coarse: None,
                        charging: ChargingState::Discharging,
                        voltage_mv: None,
                    },
                ],
                source: ReadSource::BleGattBas,
                updated_at: std::time::SystemTime::now(),
            }),
        };

        let result = correlate(vec![upower, gatt]);
        assert_eq!(result.len(), 1);
        let report = result[0].power.report().expect("should have report");
        assert_eq!(report.cells.len(), 2);
        assert_eq!(report.source, ReadSource::BleGattBas);
    }

    #[test]
    fn paired_mouse_name_beats_receiver_product_string() {
        let usb = DeviceAddress::Usb {
            vendor_id: 0x3434,
            product_id: 0xd029,
            serial: None,
            bus_addr: None,
        };
        let vendor = RawObservation {
            addresses: vec![usb.clone()],
            name: Some("Keychron M6".to_string()),
            vendor: None,
            model: None,
            vendor_id: Some(0x3434),
            product_id: Some(0xd029),
            category: DeviceCategory::Mouse,
            transport: Transport::WirelessDongle,
            power: PowerState::Unavailable {
                reason: UnavailableReason::UnsupportedProtocol,
            },
        };
        let stub = RawObservation {
            name: Some("Keychron  Keychron Link-KM".to_string()),
            category: DeviceCategory::Unknown,
            transport: Transport::Usb,
            ..vendor.clone()
        };
        let devices = correlate(vec![vendor, stub]);
        assert_eq!(devices.len(), 1);
        assert_eq!(devices[0].name, "Keychron M6");
        assert_eq!(devices[0].category, DeviceCategory::Mouse);
    }
}

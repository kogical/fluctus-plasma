// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! Async BlueZ D-Bus provider (`org.bluez.Device1`, `org.bluez.Battery1`,
//! direct BLE GATT Battery Service `0x180F`/`0x2A19`, and passive `ManufacturerData`).

use std::collections::HashMap;
use std::time::SystemTime;

use zbus::zvariant::{OwnedObjectPath, OwnedValue, Value};
use zbus::{Connection, proxy};

use crate::model::{
    BatteryCell, BatteryReport, CellRole, ChargingState, DeviceAddress, DeviceCategory,
    PowerState, RawObservation, ReadSource, Transport, UnavailableReason,
};

/// Standard Bluetooth SIG Battery Level characteristic UUID (`0x2A19`).
const GATT_BATTERY_LEVEL_UUID: &str = "00002a19-0000-1000-8000-00805f9b34fb";

type ManagedObjects = HashMap<OwnedObjectPath, HashMap<String, HashMap<String, OwnedValue>>>;

#[proxy(
    interface = "org.freedesktop.DBus.ObjectManager",
    default_service = "org.bluez",
    default_path = "/"
)]
trait BluezObjectManager {
    fn get_managed_objects(&self) -> zbus::Result<ManagedObjects>;
}

#[proxy(
    interface = "org.bluez.GattCharacteristic1",
    default_service = "org.bluez"
)]
trait GattCharacteristic {
    fn read_value(&self, options: HashMap<&str, Value<'_>>) -> zbus::Result<Vec<u8>>;
}

/// Snapshot of a connected BlueZ device, including optional manufacturer BLE data
/// so vendor modules (e.g. Apple AirPods) can inspect it without a second D-Bus round-trip.
#[derive(Debug, Clone)]
pub struct BluezDeviceSnapshot {
    pub mac: [u8; 6],
    pub name: String,
    pub uuids: Vec<String>,
    pub manufacturer_data: HashMap<u16, Vec<u8>>,
    pub(crate) observation: RawObservation,
}

/// Discovers all connected BlueZ devices (stubs + `Battery1` + direct multi-cell GATT BAS).
pub(crate) async fn discover(conn: &Connection) -> Vec<BluezDeviceSnapshot> {
    let Ok(om) = BluezObjectManagerProxy::new(conn).await else {
        return Vec::new();
    };
    let Ok(objects) = om.get_managed_objects().await else {
        return Vec::new();
    };

    // Index GATT 0x2A19 characteristics by parent device object path prefix.
    let mut gatt_battery_chars: HashMap<String, Vec<String>> = HashMap::new();
    for (path, ifaces) in &objects {
        if let Some(ch) = ifaces.get("org.bluez.GattCharacteristic1")
            && let Some(uuid) = get_string(ch, "UUID")
            && uuid.eq_ignore_ascii_case(GATT_BATTERY_LEVEL_UUID)
        {
            let p = path.as_str();
            if let Some(dev_prefix) = extract_device_prefix(p) {
                gatt_battery_chars
                    .entry(dev_prefix)
                    .or_default()
                    .push(p.to_string());
            }
        }
    }

    let mut snapshots = Vec::new();
    for (path, ifaces) in &objects {
        let Some(dev) = ifaces.get("org.bluez.Device1") else {
            continue;
        };
        if !get_bool(dev, "Connected").unwrap_or(false) {
            continue;
        }
        let Some(addr_str) = get_string(dev, "Address") else {
            continue;
        };
        let Some(mac) = DeviceAddress::parse_mac(&addr_str) else {
            continue;
        };

        let name = get_string(dev, "Alias")
            .or_else(|| get_string(dev, "Name"))
            .unwrap_or_else(|| addr_str.clone());
        let icon = get_string(dev, "Icon").unwrap_or_default();
        let addr_type = get_string(dev, "AddressType").unwrap_or_default();
        let uuids = get_string_vec(dev, "UUIDs");
        let manufacturer_data = get_manufacturer_data(dev);

        let transport = if addr_type.eq_ignore_ascii_case("random")
            || gatt_battery_chars.contains_key(path.as_str())
        {
            Transport::Ble
        } else {
            Transport::BluetoothClassic
        };

        let category = map_bluez_icon(&icon, &name);

        // 1. Try direct BLE GATT BAS (supports multi-cell devices like ZMK split keyboards).
        let mut power = PowerState::Unavailable {
            reason: UnavailableReason::UnsupportedProtocol,
        };
        if let Some(char_paths) = gatt_battery_chars.get(path.as_str()) {
            let mut sorted_paths = char_paths.clone();
            sorted_paths.sort();
            let mut cells = Vec::new();
            for (idx, char_path) in sorted_paths.iter().enumerate() {
                if let Some(pct) = read_gatt_battery_level(conn, char_path).await {
                    let role = match (sorted_paths.len(), idx) {
                        (1, _) => CellRole::Primary,
                        (2, 0) => CellRole::Left,
                        (2, 1) => CellRole::Right,
                        (3, 0) => CellRole::Left,
                        (3, 1) => CellRole::Right,
                        (3, 2) => CellRole::Case,
                        _ => CellRole::Primary,
                    };
                    cells.push(BatteryCell {
                        role,
                        percentage: Some(f32::from(pct.min(100))),
                        coarse: None,
                        charging: ChargingState::Unknown,
                        voltage_mv: None,
                    });
                }
            }
            if !cells.is_empty() {
                power = PowerState::Available(BatteryReport {
                    cells,
                    source: ReadSource::BleGattBas,
                    updated_at: SystemTime::now(),
                });
            }
        }

        // 2. Fall back to BlueZ `org.bluez.Battery1` (covers single-cell GATT + PipeWire HFP).
        if power.is_stub()
            && let Some(bat) = ifaces.get("org.bluez.Battery1")
            && let Some(pct) = get_u8(bat, "Percentage")
        {
            power = PowerState::Available(BatteryReport::single(
                f32::from(pct.min(100)),
                ChargingState::Unknown,
                ReadSource::BluezBattery1,
            ));
        }

        let observation = RawObservation {
            addresses: vec![DeviceAddress::BluetoothMac(mac)],
            name: Some(name.clone()),
            vendor: None,
            model: None,
            vendor_id: None,
            product_id: None,
            category,
            transport,
            power,
        };

        snapshots.push(BluezDeviceSnapshot {
            mac,
            name,
            uuids,
            manufacturer_data,
            observation,
        });
    }

    snapshots
}

async fn read_gatt_battery_level(conn: &Connection, char_path: &str) -> Option<u8> {
    let proxy = GattCharacteristicProxy::builder(conn)
        .path(char_path)
        .ok()?
        .build()
        .await
        .ok()?;
    let bytes = proxy.read_value(HashMap::new()).await.ok()?;
    bytes.first().copied()
}

fn extract_device_prefix(gatt_path: &str) -> Option<String> {
    // e.g. `/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/service0010/char0011`
    let idx = gatt_path.find("/service")?;
    Some(gatt_path[..idx].to_string())
}

fn map_bluez_icon(icon: &str, name: &str) -> DeviceCategory {
    match icon {
        "input-mouse" => DeviceCategory::Mouse,
        "input-keyboard" => DeviceCategory::Keyboard,
        "input-gaming" => DeviceCategory::Controller,
        "input-tablet" => DeviceCategory::Tablet,
        "audio-headset" => DeviceCategory::Headset,
        "audio-headphones" => DeviceCategory::Headphones,
        "audio-card" => DeviceCategory::Speaker,
        "phone" => DeviceCategory::Phone,
        _ => {
            #[cfg(feature = "sysfs")]
            {
                crate::providers::sysfs::infer_category_from_name(name)
            }
            #[cfg(not(feature = "sysfs"))]
            {
                let _ = name;
                DeviceCategory::Unknown
            }
        }
    }
}

fn get_string(map: &HashMap<String, OwnedValue>, key: &str) -> Option<String> {
    map.get(key)
        .and_then(|v| String::try_from(v.clone()).ok())
}

fn get_bool(map: &HashMap<String, OwnedValue>, key: &str) -> Option<bool> {
    map.get(key).and_then(|v| bool::try_from(v.clone()).ok())
}

fn get_u8(map: &HashMap<String, OwnedValue>, key: &str) -> Option<u8> {
    map.get(key).and_then(|v| u8::try_from(v.clone()).ok())
}

fn get_string_vec(map: &HashMap<String, OwnedValue>, key: &str) -> Vec<String> {
    map.get(key)
        .and_then(|v| Vec::<String>::try_from(v.clone()).ok())
        .unwrap_or_default()
}

fn get_manufacturer_data(map: &HashMap<String, OwnedValue>) -> HashMap<u16, Vec<u8>> {
    let Some(raw) = map.get("ManufacturerData") else {
        return HashMap::new();
    };
    let Ok(dict) = HashMap::<u16, OwnedValue>::try_from(raw.clone()) else {
        return HashMap::new();
    };
    let mut out = HashMap::new();
    for (company_id, val) in dict {
        if let Ok(bytes) = Vec::<u8>::try_from(val) {
            out.insert(company_id, bytes);
        }
    }
    out
}

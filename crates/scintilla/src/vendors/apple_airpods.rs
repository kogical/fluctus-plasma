// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! Apple AirPods / Beats passive BLE Continuity advertisement parser (Company ID `0x004C`).
//!
//! Apple audio devices ignore standard BLE GATT Battery Service (`0x180F`) and instead
//! broadcast Left/Right/Case battery levels inside BLE Continuity advertisements
//! (Manufacturer ID `0x004C`, type `0x07` Proximity Pairing). Because this module reads
//! BlueZ's cached `ManufacturerData`, it is completely passive and never wakes the earbuds.

use std::time::SystemTime;

use crate::model::{
    BatteryCell, BatteryReport, CellRole, ChargingState, PowerState, ReadSource,
};
#[cfg(feature = "bluez")]
use crate::providers::bluez::BluezDeviceSnapshot;
#[cfg(feature = "bluez")]
use crate::vendors::{BluetoothVendorDriver, BoxFuture};

/// Apple Bluetooth SIG Company Identifier (`0x004C`).
pub const APPLE_COMPANY_ID: u16 = 0x004C;

/// Passive BLE Continuity advertisement driver for Apple AirPods and Beats.
#[derive(Debug, Default)]
pub struct AppleAirPodsDriver;

/// Parses an Apple Proximity Pairing (`0x07`) manufacturer data payload.
///
/// Expected payload (27 bytes starting at type byte `0x07`, length `0x19`):
/// - `data[0] == 0x07`, `data[1] == 0x19`
/// - `data[5]`: status flags (bit 5 indicates left/right swap)
/// - `data[6]`: nibbles for bud 1 (high nibble) and bud 2 (low nibble), each `0..=10` (0xF = unknown)
/// - `data[7]`: low nibble = case level (`0..=10`, 0xF = unknown), high nibble = charging flags
///   (bit 4 = bud 1 charging, bit 5 = bud 2 charging, bit 6 = case charging)
#[must_use]
pub fn parse_continuity_payload(data: &[u8]) -> Option<BatteryReport> {
    if data.len() < 8 || data[0] != 0x07 || data[1] < 0x19 {
        return None;
    }

    let flipped = (data[5] & 0x20) == 0;
    let bud1_raw = (data[6] >> 4) & 0x0F;
    let bud2_raw = data[6] & 0x0F;
    let case_raw = data[7] & 0x0F;
    let chg_flags = (data[7] >> 4) & 0x0F;

    let (left_raw, right_raw, left_chg, right_chg) = if flipped {
        (
            bud2_raw,
            bud1_raw,
            (chg_flags & 0x02) != 0,
            (chg_flags & 0x01) != 0,
        )
    } else {
        (
            bud1_raw,
            bud2_raw,
            (chg_flags & 0x01) != 0,
            (chg_flags & 0x02) != 0,
        )
    };
    let case_chg = (chg_flags & 0x04) != 0;

    let mut cells = Vec::new();
    for (role, raw, is_chg) in [
        (CellRole::Left, left_raw, left_chg),
        (CellRole::Right, right_raw, right_chg),
        (CellRole::Case, case_raw, case_chg),
    ] {
        if raw <= 10 {
            let pct = f32::from(raw) * 10.0;
            cells.push(BatteryCell {
                role,
                percentage: Some(pct),
                coarse: None,
                charging: if is_chg {
                    ChargingState::Charging
                } else {
                    ChargingState::Discharging
                },
                voltage_mv: None,
            });
        }
    }

    if cells.is_empty() {
        return None;
    }

    Some(BatteryReport {
        cells,
        source: ReadSource::Vendor("apple-airpods".into()),
        updated_at: SystemTime::now(),
    })
}

#[cfg(feature = "bluez")]
impl BluetoothVendorDriver for AppleAirPodsDriver {
    fn id(&self) -> &'static str {
        "apple-airpods"
    }

    fn matches(&self, snap: &BluezDeviceSnapshot) -> bool {
        snap.manufacturer_data.contains_key(&APPLE_COMPANY_ID)
            || snap.name.to_ascii_lowercase().contains("airpods")
            || snap.name.to_ascii_lowercase().contains("beats")
    }

    fn read_power<'a>(
        &'a self,
        snap: &'a BluezDeviceSnapshot,
    ) -> BoxFuture<'a, Option<PowerState>> {
        Box::pin(async move {
            let payload = snap.manufacturer_data.get(&APPLE_COMPANY_ID)?;
            let report = parse_continuity_payload(payload)?;
            Some(PowerState::Available(report))
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_left_right_case_continuity_packet() {
        let mut payload = [0u8; 27];
        payload[0] = 0x07;
        payload[1] = 0x19;
        payload[5] = 0x20; // not flipped: bud1=left, bud2=right
        payload[6] = (9 << 4) | 8; // left=90%, right=80%
        payload[7] = (0x04 << 4) | 6; // case=60%, case charging
        let report = parse_continuity_payload(&payload).unwrap();
        assert_eq!(report.cells.len(), 3);
        assert_eq!(report.cells[0].role, CellRole::Left);
        assert_eq!(report.cells[0].percentage, Some(90.0));
        assert_eq!(report.cells[1].role, CellRole::Right);
        assert_eq!(report.cells[1].percentage, Some(80.0));
        assert_eq!(report.cells[2].role, CellRole::Case);
        assert_eq!(report.cells[2].percentage, Some(60.0));
        assert_eq!(report.cells[2].charging, ChargingState::Charging);
    }
}

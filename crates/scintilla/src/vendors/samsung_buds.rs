// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! Samsung Galaxy Buds RFCOMM/SPP binary protocol driver.
//!
//! Best-effort implementation based on `GalaxyBudsClient`. Samsung Galaxy Buds
//! expose Left/Right/Case battery inside `MSG_ID_EXTENDED_STATUS_UPDATED` (`0x61`)
//! frames over RFCOMM (`SOM = 0xFD`, `EOM = 0xDD`).

use std::time::{Duration, SystemTime};

use crate::model::{
    BatteryCell, BatteryReport, CellRole, ChargingState, PowerState, ReadSource,
};
#[cfg(feature = "bluez")]
use crate::providers::bluez::BluezDeviceSnapshot;
#[cfg(feature = "rfcomm")]
use crate::providers::rfcomm::rfcomm_exchange;
#[cfg(feature = "bluez")]
use crate::vendors::{BluetoothVendorDriver, BoxFuture};

/// Samsung Galaxy Buds SPP UUID.
pub const SAMSUNG_BUDS_SPP_UUID: &str = "00001101-0000-1000-8000-00805f9b34fb";

const SOM: u8 = 0xFD;
const EOM: u8 = 0xDD;
const MSG_EXTENDED_STATUS_UPDATED: u8 = 0x61;

/// Vendor driver for Samsung Galaxy Buds.
#[derive(Debug, Default)]
pub struct SamsungBudsDriver;

/// Parses a Samsung Galaxy Buds `0x61` extended status RFCOMM frame.
///
/// Expected payload starting after `[SOM, size_lo, size_hi, msg_id=0x61]`:
/// - `payload[0]`: protocol revision
/// - `payload[1]`: left bud battery (`0..=100`, `0` if disconnected)
/// - `payload[2]`: right bud battery (`0..=100`, `0` if disconnected)
/// - `payload[7]`: charging case battery (`0..=100`, `101+` if unknown)
#[must_use]
pub fn parse_extended_status_frame(frame: &[u8]) -> Option<BatteryReport> {
    let start = frame.iter().position(|&b| b == SOM)?;
    let end = frame[start + 1..]
        .iter()
        .position(|&b| b == EOM)
        .map(|i| start + 1 + i)?;
    let inner = &frame[start + 1..end];
    if inner.len() < 11 {
        return None;
    }

    let msg_id = inner[2];
    if msg_id != MSG_EXTENDED_STATUS_UPDATED {
        return None;
    }
    let payload = &inner[3..];
    if payload.len() < 8 {
        return None;
    }

    let mut cells = Vec::new();
    for (role, raw) in [
        (CellRole::Left, payload[1]),
        (CellRole::Right, payload[2]),
        (CellRole::Case, payload[7]),
    ] {
        if (1..=100).contains(&raw) {
            cells.push(BatteryCell {
                role,
                percentage: Some(f32::from(raw)),
                coarse: None,
                charging: ChargingState::Unknown,
                voltage_mv: None,
            });
        }
    }

    if cells.is_empty() {
        return None;
    }

    Some(BatteryReport {
        cells,
        source: ReadSource::Vendor("samsung-buds".into()),
        updated_at: SystemTime::now(),
    })
}

#[cfg(feature = "bluez")]
impl BluetoothVendorDriver for SamsungBudsDriver {
    fn id(&self) -> &'static str {
        "samsung-buds"
    }

    fn matches(&self, snap: &BluezDeviceSnapshot) -> bool {
        snap.name.to_ascii_lowercase().contains("galaxy buds")
    }

    fn read_power<'a>(
        &'a self,
        snap: &'a BluezDeviceSnapshot,
    ) -> BoxFuture<'a, Option<PowerState>> {
        Box::pin(async move {
            #[cfg(feature = "rfcomm")]
            {
                // Galaxy Buds push `0x61` shortly after RFCOMM connect on channel 1 or 5.
                for ch in [1u8, 5u8] {
                    if let Ok(resp) =
                        rfcomm_exchange(snap.mac, ch, Vec::new(), 256, Duration::from_millis(800))
                            .await
                        && let Some(report) = parse_extended_status_frame(&resp)
                    {
                        return Some(PowerState::Available(report));
                    }
                }
            }
            let _ = snap;
            None
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_galaxy_buds_extended_status() {
        // [SOM, len_lo, len_hi, 0x61, rev, left=88, right=84, 0, 0, 0, 0, case=70, crc_lo, crc_hi, EOM]
        let frame = [
            SOM, 0x0A, 0x00, 0x61, 0x02, 88, 84, 0, 0, 0, 0, 70, 0x00, 0x00, EOM,
        ];
        let report = parse_extended_status_frame(&frame).unwrap();
        assert_eq!(report.cells.len(), 3);
        assert_eq!(report.cells[0].percentage, Some(88.0));
        assert_eq!(report.cells[1].percentage, Some(84.0));
        assert_eq!(report.cells[2].percentage, Some(70.0));
    }
}

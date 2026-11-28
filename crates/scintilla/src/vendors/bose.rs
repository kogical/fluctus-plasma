// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! Bose Mobile Audio Protocol (BMAP) RFCOMM battery driver.
//!
//! Best-effort implementation for Bose QuietComfort / Ultra headphones using
//! BMAP `GetBatteryLevel` (`[0x02, 0x02, 0x01, 0x00]`).

use std::time::Duration;

use crate::model::{BatteryReport, ChargingState, PowerState, ReadSource};
#[cfg(feature = "bluez")]
use crate::providers::bluez::BluezDeviceSnapshot;
#[cfg(feature = "rfcomm")]
use crate::providers::rfcomm::rfcomm_exchange;
#[cfg(feature = "bluez")]
use crate::vendors::{BluetoothVendorDriver, BoxFuture};

/// Vendor driver for Bose BMAP headphones.
#[derive(Debug, Default)]
pub struct BoseBmapDriver;

/// Builds the BMAP `GetBatteryLevel` request (`block=0x02, func=0x02, op=0x01 GET, len=0x00`).
#[must_use]
pub fn build_get_battery_level() -> Vec<u8> {
    vec![0x02, 0x02, 0x01, 0x00]
}

/// Parses a BMAP `BatteryLevel` status packet (`[0x02, 0x02, 0x03, 0x01, pct]`).
#[must_use]
pub fn parse_battery_level(resp: &[u8]) -> Option<BatteryReport> {
    for win in resp.windows(5) {
        if win[0] == 0x02 && win[1] == 0x02 && win[2] == 0x03 && win[3] >= 0x01 {
            let pct = win[4];
            if pct <= 100 {
                return Some(BatteryReport::single(
                    f32::from(pct),
                    ChargingState::Unknown,
                    ReadSource::Vendor("bose-bmap".into()),
                ));
            }
        }
    }
    None
}

#[cfg(feature = "bluez")]
impl BluetoothVendorDriver for BoseBmapDriver {
    fn id(&self) -> &'static str {
        "bose-bmap"
    }

    fn matches(&self, snap: &BluezDeviceSnapshot) -> bool {
        let lower = snap.name.to_ascii_lowercase();
        lower.contains("bose") || lower.contains("quietcomfort") || lower.contains("qc35")
    }

    fn read_power<'a>(
        &'a self,
        snap: &'a BluezDeviceSnapshot,
    ) -> BoxFuture<'a, Option<PowerState>> {
        Box::pin(async move {
            #[cfg(feature = "rfcomm")]
            {
                let req = build_get_battery_level();
                // Bose BMAP typically listens on RFCOMM channel 8 or 1.
                for ch in [8u8, 1u8] {
                    if let Ok(resp) =
                        rfcomm_exchange(snap.mac, ch, req.clone(), 64, Duration::from_millis(600))
                            .await
                        && let Some(report) = parse_battery_level(&resp)
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
    fn parses_bmap_battery_response() {
        let report = parse_battery_level(&[0x02, 0x02, 0x03, 0x01, 60]).unwrap();
        assert_eq!(report.primary_percentage(), Some(60.0));
    }
}

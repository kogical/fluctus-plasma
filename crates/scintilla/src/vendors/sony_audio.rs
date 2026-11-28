// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! Sony WH-1000XM / WF-1000XM RFCOMM/SPP battery driver.
//!
//! Unlike Sony's PlayStation controllers (which have an official mainline Linux
//! kernel driver), Sony's audio division uses a custom framed binary protocol
//! over Classic Bluetooth RFCOMM/SPP (`SONY_SPP_UUID`).
//!
//! Best-effort implementation based on `sony-headphones-control-linux`.

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

/// Sony Headphones Connect SPP service UUID.
pub const SONY_SPP_UUID: &str = "96cc203e-5068-46ad-b32d-e316f5e069ba";

const START_MARKER: u8 = 0x3E;
const END_MARKER: u8 = 0x3C;

/// Vendor driver for Sony WH/WF-1000XM series Bluetooth audio devices.
#[derive(Debug, Default)]
pub struct SonyAudioDriver;

/// Builds a Sony Headphones Protocol battery-get frame (`cmd = 0x10`, `inquired_type = 0x00`).
#[must_use]
pub fn build_battery_request(dual: bool) -> Vec<u8> {
    // Payload: [0x10 (BATTERY_GET), 0x00 (single) or 0x01 (dual L/R)]
    let payload = [0x10u8, u8::from(dual)];
    let data_type: u8 = 0x0C; // DATA_MDR
    let seq: u8 = 0x00;
    let len_bytes = (payload.len() as u32).to_be_bytes();

    let mut body = Vec::with_capacity(6 + payload.len());
    body.push(data_type);
    body.push(seq);
    body.extend_from_slice(&len_bytes);
    body.extend_from_slice(&payload);

    let checksum = body.iter().fold(0u8, |acc, &b| acc.wrapping_add(b));

    let mut frame = Vec::with_capacity(body.len() + 3);
    frame.push(START_MARKER);
    frame.extend_from_slice(&body);
    frame.push(checksum);
    frame.push(END_MARKER);
    frame
}

/// Parses a Sony Headphones Protocol battery-ret (`0x11`) response frame.
#[must_use]
pub fn parse_battery_response(frame: &[u8]) -> Option<BatteryReport> {
    let start = frame.iter().position(|&b| b == START_MARKER)?;
    let end = frame[start + 1..]
        .iter()
        .position(|&b| b == END_MARKER)
        .map(|i| start + 1 + i)?;
    let inner = &frame[start + 1..end];
    if inner.len() < 9 {
        return None;
    }

    let expected_sum = *inner.last()?;
    let body = &inner[..inner.len() - 1];
    let actual_sum = body.iter().fold(0u8, |acc, &b| acc.wrapping_add(b));
    if actual_sum != expected_sum {
        return None;
    }

    let payload = &body[6..];
    if payload.len() < 4 || payload[0] != 0x11 {
        return None;
    }

    let inquired_type = payload[1];
    let mut cells = Vec::new();
    match inquired_type {
        // Single battery (over-ear WH-1000XM)
        0x00 => {
            let pct = payload[2];
            if pct <= 100 {
                cells.push(BatteryCell {
                    role: CellRole::Primary,
                    percentage: Some(f32::from(pct)),
                    coarse: None,
                    charging: if payload[3] != 0 {
                        ChargingState::Charging
                    } else {
                        ChargingState::Discharging
                    },
                    voltage_mv: None,
                });
            }
        }
        // Dual earbuds (WF-1000XM: L + R, optional Case)
        0x01 if payload.len() >= 6 => {
            for (role, pct, chg) in [
                (CellRole::Left, payload[2], payload[3]),
                (CellRole::Right, payload[4], payload[5]),
            ] {
                if pct <= 100 {
                    cells.push(BatteryCell {
                        role,
                        percentage: Some(f32::from(pct)),
                        coarse: None,
                        charging: if chg != 0 {
                            ChargingState::Charging
                        } else {
                            ChargingState::Discharging
                        },
                        voltage_mv: None,
                    });
                }
            }
            if payload.len() >= 8 && payload[6] <= 100 {
                cells.push(BatteryCell {
                    role: CellRole::Case,
                    percentage: Some(f32::from(payload[6])),
                    coarse: None,
                    charging: if payload[7] != 0 {
                        ChargingState::Charging
                    } else {
                        ChargingState::Discharging
                    },
                    voltage_mv: None,
                });
            }
        }
        _ => return None,
    }

    if cells.is_empty() {
        return None;
    }

    Some(BatteryReport {
        cells,
        source: ReadSource::Vendor("sony-rfcomm".into()),
        updated_at: SystemTime::now(),
    })
}

#[cfg(feature = "bluez")]
impl BluetoothVendorDriver for SonyAudioDriver {
    fn id(&self) -> &'static str {
        "sony-rfcomm"
    }

    fn matches(&self, snap: &BluezDeviceSnapshot) -> bool {
        snap.uuids
            .iter()
            .any(|u| u.eq_ignore_ascii_case(SONY_SPP_UUID))
            || snap.name.contains("WH-1000")
            || snap.name.contains("WF-1000")
    }

    fn read_power<'a>(
        &'a self,
        snap: &'a BluezDeviceSnapshot,
    ) -> BoxFuture<'a, Option<PowerState>> {
        Box::pin(async move {
            #[cfg(feature = "rfcomm")]
            {
                let is_buds = snap.name.contains("WF-");
                let req = build_battery_request(is_buds);
                // Sony SPP commonly binds to RFCOMM channel 9 or 1.
                for ch in [9u8, 1u8] {
                    if let Ok(resp) =
                        rfcomm_exchange(snap.mac, ch, req.clone(), 128, Duration::from_millis(600))
                            .await
                        && let Some(report) = parse_battery_response(&resp)
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

    fn wrap_frame(payload: &[u8]) -> Vec<u8> {
        let mut body = vec![0x0C, 0x00];
        body.extend_from_slice(&(payload.len() as u32).to_be_bytes());
        body.extend_from_slice(payload);
        let sum = body.iter().fold(0u8, |a, &b| a.wrapping_add(b));
        let mut out = vec![START_MARKER];
        out.extend_from_slice(&body);
        out.push(sum);
        out.push(END_MARKER);
        out
    }

    #[test]
    fn parses_single_and_dual_sony_responses() {
        let single = wrap_frame(&[0x11, 0x00, 80, 0x00]);
        let r1 = parse_battery_response(&single).unwrap();
        assert_eq!(r1.primary_percentage(), Some(80.0));

        let dual = wrap_frame(&[0x11, 0x01, 90, 0x00, 85, 0x00, 50, 0x01]);
        let r2 = parse_battery_response(&dual).unwrap();
        assert_eq!(r2.cells.len(), 3);
        assert_eq!(r2.cells[2].charging, ChargingState::Charging);
    }
}

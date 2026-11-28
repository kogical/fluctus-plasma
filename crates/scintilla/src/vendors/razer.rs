// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! Razer 90-byte USB HID Feature Report battery driver (VID `0x1532`).
//!
//! Razer wireless mice and keyboards require a 90-byte feature report with a
//! transaction ID, command class (`0x07` power), command ID (`0x80` battery level,
//! `0x84` charging status), and an XOR checksum of bytes `2..=87` in byte `88`.
//!
//! Best-effort implementation based on `OpenRazer` protocol documentation.

use std::time::Duration;

use crate::error::Error;
use crate::model::{
    BatteryReport, ChargingState, PowerState, ReadSource, UnavailableReason,
};
#[cfg(all(feature = "hidraw", feature = "sysfs"))]
use crate::providers::hidraw::HidrawDevice;
#[cfg(all(feature = "hidraw", feature = "sysfs"))]
use crate::providers::sysfs::HidrawDeviceInfo;
#[cfg(all(feature = "hidraw", feature = "sysfs"))]
use crate::vendors::{BoxFuture, HidVendorDriver};

/// Razer USB Vendor ID.
pub const VENDOR_ID: u16 = 0x1532;

const REPORT_LEN: usize = 90;

/// Vendor driver for Razer wireless peripherals.
#[derive(Debug, Default)]
pub struct RazerDriver;

/// Computes the Razer XOR checksum over bytes `2..=87` of a 90-byte report.
#[must_use]
pub fn compute_crc(report: &[u8]) -> u8 {
    report
        .get(2..88)
        .unwrap_or(&[])
        .iter()
        .fold(0u8, |acc, &b| acc ^ b)
}

/// Builds a 91-byte buffer (leading Report ID `0x00` + 90-byte Razer packet).
#[must_use]
pub fn build_razer_request(transaction_id: u8, command_class: u8, command_id: u8) -> Vec<u8> {
    let mut pkt = [0u8; REPORT_LEN];
    pkt[0] = 0x00; // status: new command
    pkt[1] = transaction_id;
    pkt[5] = 0x02; // data size
    pkt[6] = command_class;
    pkt[7] = command_id;
    pkt[88] = compute_crc(&pkt);

    let mut out = Vec::with_capacity(REPORT_LEN + 1);
    out.push(0x00); // HID Report ID
    out.extend_from_slice(&pkt);
    out
}

/// Parses a Razer battery level response (accepting either 90-byte raw or 91-byte with Report ID).
#[must_use]
pub fn parse_battery_response(raw: &[u8]) -> Option<f32> {
    let pkt = if raw.len() == REPORT_LEN + 1 {
        &raw[1..]
    } else if raw.len() >= REPORT_LEN {
        &raw[..REPORT_LEN]
    } else {
        return None;
    };

    // Byte 0: 0x02 = successful response
    if pkt[0] != 0x02 || pkt[88] != compute_crc(pkt) {
        return None;
    }
    // Raw battery level in byte 9 is scaled 0..=255.
    let level = pkt[9];
    if level == 0 {
        return None;
    }
    Some((f32::from(level) * 100.0 / 255.0).round())
}

/// Parses a Razer charging status response (`command_id = 0x84`).
#[must_use]
pub fn parse_charging_response(raw: &[u8]) -> ChargingState {
    let pkt = if raw.len() == REPORT_LEN + 1 {
        &raw[1..]
    } else if raw.len() >= REPORT_LEN {
        &raw[..REPORT_LEN]
    } else {
        return ChargingState::Unknown;
    };
    if pkt[0] != 0x02 || pkt[88] != compute_crc(pkt) {
        return ChargingState::Unknown;
    }
    match pkt[9] {
        1 => ChargingState::Charging,
        0 => ChargingState::Discharging,
        _ => ChargingState::Unknown,
    }
}

#[cfg(all(feature = "hidraw", feature = "sysfs"))]
impl HidVendorDriver for RazerDriver {
    fn id(&self) -> &'static str {
        "razer"
    }

    fn matches(&self, info: &HidrawDeviceInfo) -> bool {
        info.vendor_id == VENDOR_ID
    }

    fn read_power<'a>(
        &'a self,
        _info: &'a HidrawDeviceInfo,
        dev: &'a HidrawDevice,
    ) -> BoxFuture<'a, Option<PowerState>> {
        Box::pin(async move {
            // Try common Razer transaction IDs (0x1F for newer HyperSpeed, 0x3F for legacy).
            for tx_id in [0x1Fu8, 0x3Fu8] {
                let req = build_razer_request(tx_id, 0x07, 0x80);
                if let Err(err) = dev.set_feature_report(req).await {
                    if let Error::PermissionDenied { path } = err {
                        return Some(PowerState::Unavailable {
                            reason: UnavailableReason::PermissionDenied { path },
                        });
                    }
                    continue;
                }
                tokio::time::sleep(Duration::from_millis(35)).await;
                let Ok(resp) = dev.get_feature_report(vec![0u8; REPORT_LEN + 1]).await else {
                    continue;
                };
                if let Some(pct) = parse_battery_response(&resp) {
                    // Best-effort charging state query (command 0x84).
                    let mut charging = ChargingState::Unknown;
                    let chg_req = build_razer_request(tx_id, 0x07, 0x84);
                    if dev.set_feature_report(chg_req).await.is_ok() {
                        tokio::time::sleep(Duration::from_millis(35)).await;
                        if let Ok(chg_resp) =
                            dev.get_feature_report(vec![0u8; REPORT_LEN + 1]).await
                        {
                            charging = parse_charging_response(&chg_resp);
                        }
                    }
                    return Some(PowerState::Available(BatteryReport::single(
                        pct,
                        charging,
                        ReadSource::Vendor("razer".into()),
                    )));
                }
            }
            None
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn validates_checksum_and_scales_battery() {
        let mut pkt = [0u8; REPORT_LEN];
        pkt[0] = 0x02;
        pkt[6] = 0x07;
        pkt[7] = 0x80;
        pkt[9] = 128; // ~50%
        pkt[88] = compute_crc(&pkt);
        assert_eq!(parse_battery_response(&pkt), Some(50.0));

        // Corrupt checksum must fail
        pkt[88] ^= 0xFF;
        assert_eq!(parse_battery_response(&pkt), None);
    }
}

// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! SteelSeries Arctis / Nova / Aerox HID battery driver (VID `0x1038`).
//!
//! Best-effort implementation covering the `[0x06, 0x18]` (Arctis 7/1/Pro) and
//! `[0x06, 0xB0]` (Arctis Nova 7/Pro) `hidraw` queries documented by `HeadsetControl`
//! and `hid-steelseries`.

use std::time::Duration;

use crate::error::Error;
use crate::model::{
    BatteryCell, BatteryReport, CellRole, ChargingState, CoarseLevel, PowerState, ReadSource,
    UnavailableReason,
};
#[cfg(all(feature = "hidraw", feature = "sysfs"))]
use crate::providers::hidraw::HidrawDevice;
#[cfg(all(feature = "hidraw", feature = "sysfs"))]
use crate::providers::sysfs::HidrawDeviceInfo;
#[cfg(all(feature = "hidraw", feature = "sysfs"))]
use crate::vendors::{BoxFuture, HidVendorDriver};

/// SteelSeries USB Vendor ID.
pub const VENDOR_ID: u16 = 0x1038;

/// Vendor driver for SteelSeries wireless headsets and mice.
#[derive(Debug, Default)]
pub struct SteelSeriesDriver;

/// Builds a SteelSeries Arctis/Nova battery query (`[0x06, subcmd]`).
#[must_use]
pub fn build_query(subcmd: u8) -> Vec<u8> {
    let mut pkt = vec![0u8; 64];
    pkt[0] = 0x06;
    pkt[1] = subcmd;
    pkt
}

/// Parses a SteelSeries Arctis (`0x06 0x18`) response.
#[must_use]
pub fn parse_arctis_response(resp: &[u8]) -> Option<PowerState> {
    if resp.len() < 4 || resp[0] != 0x06 || resp[1] != 0x18 {
        return None;
    }
    let pct = resp[2];
    if pct == 0 {
        return Some(PowerState::Unavailable {
            reason: UnavailableReason::DeviceAsleep,
        });
    }
    if pct > 100 {
        return None;
    }
    let charging = match resp[3] {
        1 => ChargingState::Charging,
        0 => ChargingState::Discharging,
        _ => ChargingState::Unknown,
    };
    Some(PowerState::Available(BatteryReport::single(
        f32::from(pct),
        charging,
        ReadSource::Vendor("steelseries".into()),
    )))
}

/// Parses a SteelSeries Nova (`0x06 0xB0`) response (0..=4 discrete bars + charging byte).
#[must_use]
pub fn parse_nova_response(resp: &[u8]) -> Option<PowerState> {
    if resp.len() < 5 || resp[0] != 0x06 || resp[1] != 0xB0 {
        return None;
    }
    if resp[2] == 0 {
        return Some(PowerState::Unavailable {
            reason: UnavailableReason::DeviceAsleep,
        });
    }
    let bars = resp[3].min(4);
    let (pct, coarse) = match bars {
        0 => (10.0, CoarseLevel::Critical),
        1 => (25.0, CoarseLevel::Low),
        2 => (50.0, CoarseLevel::Normal),
        3 => (75.0, CoarseLevel::High),
        _ => (100.0, CoarseLevel::Full),
    };
    let charging = match resp[4] {
        1 => ChargingState::Charging,
        3 => ChargingState::Discharging,
        _ => ChargingState::Unknown,
    };
    Some(PowerState::Available(BatteryReport {
        cells: vec![BatteryCell {
            role: CellRole::Primary,
            percentage: Some(pct),
            coarse: Some(coarse),
            charging,
            voltage_mv: None,
        }],
        source: ReadSource::Vendor("steelseries-nova".into()),
        updated_at: std::time::SystemTime::now(),
    }))
}

#[cfg(all(feature = "hidraw", feature = "sysfs"))]
impl HidVendorDriver for SteelSeriesDriver {
    fn id(&self) -> &'static str {
        "steelseries"
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
            for (subcmd, is_nova) in [(0x18u8, false), (0xB0u8, true)] {
                match dev
                    .exchange_report(build_query(subcmd), 64, Duration::from_millis(250))
                    .await
                {
                    Ok(resp) => {
                        let parsed = if is_nova {
                            parse_nova_response(&resp)
                        } else {
                            parse_arctis_response(&resp)
                        };
                        if parsed.is_some() {
                            return parsed;
                        }
                    }
                    Err(Error::PermissionDenied { path }) => {
                        return Some(PowerState::Unavailable {
                            reason: UnavailableReason::PermissionDenied { path },
                        });
                    }
                    Err(_) => continue,
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
    fn parses_arctis_and_nova() {
        let a = parse_arctis_response(&[0x06, 0x18, 92, 0x00]).unwrap();
        assert_eq!(a.report().unwrap().primary_percentage(), Some(92.0));

        let n = parse_nova_response(&[0x06, 0xB0, 0x03, 3, 1]).unwrap();
        let report = n.report().unwrap();
        assert_eq!(report.primary_percentage(), Some(75.0));
        assert_eq!(report.cells[0].coarse, Some(CoarseLevel::High));
        assert_eq!(report.cells[0].charging, ChargingState::Charging);
    }
}

// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! Corsair Slipstream / iCUE read-only HID battery driver (VID `0x1B1C`).
//!
//! Strictly uses read-only query opcodes (`0x0E` / `0xC9`) and **never** issues
//! `0x07` write commands (which can trigger factory resets on Corsair firmware).
//!
//! Best-effort implementation based on `ckb-next` and `HeadsetControl`.

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

/// Corsair USB Vendor ID.
pub const VENDOR_ID: u16 = 0x1B1C;

/// Vendor driver for Corsair wireless peripherals.
#[derive(Debug, Default)]
pub struct CorsairDriver;

/// Builds a read-only Corsair Void/Virtuoso headset battery query (`0xC9 0x64`).
#[must_use]
pub fn build_headset_battery_query() -> Vec<u8> {
    let mut pkt = vec![0u8; 64];
    pkt[0] = 0xC9;
    pkt[1] = 0x64;
    pkt
}

/// Builds a read-only Corsair Slipstream (`0x0E 0x50`) battery query.
#[must_use]
pub fn build_slipstream_battery_query() -> Vec<u8> {
    let mut pkt = vec![0u8; 64];
    pkt[0] = 0x0E;
    pkt[1] = 0x50;
    pkt
}

/// Parses a Corsair Void/Virtuoso (`0x64`) battery response.
#[must_use]
pub fn parse_headset_response(resp: &[u8]) -> Option<PowerState> {
    if resp.len() < 5 || resp[0] != 0x64 {
        return None;
    }
    let state_byte = resp[4];
    // Bit 7 (0x80) indicates charging on Void/Virtuoso firmware; lower 7 bits are percentage.
    let raw_pct = resp[2] & 0x7F;
    if raw_pct == 0 && state_byte == 0 {
        return Some(PowerState::Unavailable {
            reason: UnavailableReason::DeviceAsleep,
        });
    }
    if raw_pct > 100 {
        return None;
    }
    let charging = match state_byte {
        0x04 | 0x05 => ChargingState::Charging,
        0x01 => ChargingState::Discharging,
        _ if (resp[2] & 0x80) != 0 => ChargingState::Charging,
        _ => ChargingState::Unknown,
    };
    Some(PowerState::Available(BatteryReport::single(
        f32::from(raw_pct),
        charging,
        ReadSource::Vendor("corsair".into()),
    )))
}

/// Parses a Corsair Slipstream (`0x0E 0x50`) response (0..=1000 tenths of a percent).
#[must_use]
pub fn parse_slipstream_response(resp: &[u8]) -> Option<PowerState> {
    if resp.len() < 6 || resp[0] != 0x0E || resp[1] != 0x50 {
        return None;
    }
    let tenths = u16::from_le_bytes([resp[4], resp[5]]);
    if tenths == 0 || tenths > 1000 {
        return None;
    }
    Some(PowerState::Available(BatteryReport::single(
        f32::from(tenths) / 10.0,
        ChargingState::Unknown,
        ReadSource::Vendor("corsair-slipstream".into()),
    )))
}

#[cfg(all(feature = "hidraw", feature = "sysfs"))]
impl HidVendorDriver for CorsairDriver {
    fn id(&self) -> &'static str {
        "corsair"
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
            match dev
                .exchange_report(build_headset_battery_query(), 64, Duration::from_millis(250))
                .await
            {
                Ok(resp) => {
                    if let Some(state) = parse_headset_response(&resp) {
                        return Some(state);
                    }
                }
                Err(Error::PermissionDenied { path }) => {
                    return Some(PowerState::Unavailable {
                        reason: UnavailableReason::PermissionDenied { path },
                    });
                }
                Err(_) => {}
            }

            if let Ok(resp) = dev
                .exchange_report(
                    build_slipstream_battery_query(),
                    64,
                    Duration::from_millis(250),
                )
                .await
                && let Some(state) = parse_slipstream_response(&resp)
            {
                return Some(state);
            }

            None
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_headset_and_slipstream() {
        let hs = parse_headset_response(&[0x64, 0x00, 0x80 | 65, 0x00, 0x04]).unwrap();
        let report = hs.report().unwrap();
        assert_eq!(report.primary_percentage(), Some(65.0));
        assert_eq!(report.cells[0].charging, ChargingState::Charging);

        // 825 tenths = 82.5%
        let [lo, hi] = 825u16.to_le_bytes();
        let ss = parse_slipstream_response(&[0x0E, 0x50, 0x00, 0x00, lo, hi]).unwrap();
        assert_eq!(ss.report().unwrap().primary_percentage(), Some(82.5));
    }
}

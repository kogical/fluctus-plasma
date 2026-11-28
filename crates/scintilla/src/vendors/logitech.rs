// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! Logitech HID++ 2.0 fallback driver (VID `0x046D`).
//!
//! Linux's mainline `hid-logitech-hidpp` driver handles many Unifying/Bolt/Lightspeed
//! devices via `sysfs`, so `scintilla` always prefers `sysfs` when present. This
//! userspace fallback queries HID++ 2.0 feature `0x1004` (Unified Battery) or
//! `0x1000` (Battery Status) for newer PIDs or receivers not yet covered by the kernel.

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

/// Logitech USB Vendor ID.
pub const VENDOR_ID: u16 = 0x046D;

/// HID++ 2.0 feature IDs for battery telemetry.
pub const FEATURE_BATTERY_STATUS: u16 = 0x1000;
pub const FEATURE_UNIFIED_BATTERY: u16 = 0x1004;

/// Software ID nibble (`0x0A`) embedded in HID++ requests to recognize our replies.
const SW_ID: u8 = 0x0A;

/// Vendor driver for Logitech HID++ 2.0 devices.
#[derive(Debug, Default)]
pub struct LogitechHidppDriver;

/// Builds a 7-byte HID++ 2.0 short request to resolve a 16-bit feature ID via Root (`0x00`).
#[must_use]
pub fn build_feature_lookup(device_index: u8, feature_id: u16) -> Vec<u8> {
    let [hi, lo] = feature_id.to_be_bytes();
    vec![0x10, device_index, 0x00, SW_ID, hi, lo, 0x00]
}

/// Builds a 7-byte HID++ 2.0 short request calling function `func` on `feature_index`.
#[must_use]
pub fn build_feature_call(device_index: u8, feature_index: u8, func: u8) -> Vec<u8> {
    vec![
        0x10,
        device_index,
        feature_index,
        (func << 4) | SW_ID,
        0x00,
        0x00,
        0x00,
    ]
}

/// Parses a HID++ 2.0 Unified Battery (`0x1004`, func `1`) or Battery Status (`0x1000`, func `0`) reply.
#[must_use]
pub fn parse_battery_reply(resp: &[u8], unified: bool) -> Option<PowerState> {
    if resp.len() < 7 || (resp[0] != 0x10 && resp[0] != 0x11) {
        return None;
    }
    // HID++ error frame has feature_index == 0xFF
    if resp[2] == 0xFF {
        return None;
    }

    let pct = resp[4];
    if pct > 100 {
        return None;
    }

    let charging = if unified {
        // Unified Battery (0x1004): byte 6 charging status
        match resp[6] {
            0 => ChargingState::Discharging,
            1 | 2 => ChargingState::Charging,
            3 => ChargingState::Full,
            _ => ChargingState::Unknown,
        }
    } else {
        // Battery Status (0x1000): byte 6 status
        match resp[6] {
            0 => ChargingState::Discharging,
            1 | 2 => ChargingState::Charging,
            3 => ChargingState::Full,
            _ => ChargingState::Unknown,
        }
    };

    Some(PowerState::Available(BatteryReport::single(
        f32::from(pct),
        charging,
        ReadSource::Vendor("logitech-hidpp".into()),
    )))
}

#[cfg(all(feature = "hidraw", feature = "sysfs"))]
impl HidVendorDriver for LogitechHidppDriver {
    fn id(&self) -> &'static str {
        "logitech-hidpp"
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
            // Try direct device index 0xFF (USB/BT) then receiver slot 0x01.
            for dev_idx in [0xFFu8, 0x01u8] {
                for (feature_id, func, unified) in [
                    (FEATURE_UNIFIED_BATTERY, 1u8, true),
                    (FEATURE_BATTERY_STATUS, 0u8, false),
                ] {
                    let lookup = build_feature_lookup(dev_idx, feature_id);
                    let resp = match dev
                        .exchange_report(lookup, 20, Duration::from_millis(200))
                        .await
                    {
                        Ok(r) => r,
                        Err(Error::PermissionDenied { path }) => {
                            return Some(PowerState::Unavailable {
                                reason: UnavailableReason::PermissionDenied { path },
                            });
                        }
                        Err(_) => continue,
                    };
                    let Some(&feat_idx) = resp.get(4) else {
                        continue;
                    };
                    if feat_idx == 0 {
                        continue;
                    }
                    let query = build_feature_call(dev_idx, feat_idx, func);
                    if let Ok(bat_resp) = dev
                        .exchange_report(query, 20, Duration::from_millis(200))
                        .await
                        && let Some(state) = parse_battery_reply(&bat_resp, unified)
                    {
                        return Some(state);
                    }
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
    fn parses_unified_battery_reply() {
        let resp = [0x10, 0xFF, 0x07, 0x1A, 73, 0x04, 0x01];
        let state = parse_battery_reply(&resp, true).unwrap();
        let report = state.report().unwrap();
        assert_eq!(report.primary_percentage(), Some(73.0));
        assert_eq!(report.cells[0].charging, ChargingState::Charging);
    }
}

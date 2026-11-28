// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! Keychron wireless mice & 2.4 GHz Link dongle battery driver (VID `0x3434`).
//!
//! Over Bluetooth, Keychron peripherals generally use standard BLE GATT BAS (`0x180F`).
//! Over USB-C or the 2.4 GHz Keychron Link dongle, they instead expose vendor-specific
//! HID reports on a secondary interface (Report ID `0xB3`/`0xB4` with command `0x06`,
//! or Feature Report `0x51`/`0x52` depending on firmware generation).
//!
//! Best-effort implementation based on community reverse-engineering (`keychron-battery-dkms`,
//! `keychron-battery`).

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

/// Keychron USB Vendor ID.
pub const VENDOR_ID: u16 = 0x3434;

/// Vendor driver for Keychron mice and 2.4 GHz wireless dongles.
#[derive(Debug, Default)]
pub struct KeychronDriver;

/// Builds the 64-byte Keychron M-series battery query packet (Report ID `0xB3`, cmd `0x06`).
#[must_use]
pub fn build_b3_battery_request() -> Vec<u8> {
    let mut pkt = vec![0u8; 64];
    pkt[0] = 0xB3;
    pkt[1] = 0x06;
    pkt
}

/// Parses a Keychron `0xB4` battery response packet.
///
/// Expected layout (community-documented across M2/M3/M4/M6 Link dongles):
/// - `resp[0] == 0xB4` (response report ID)
/// - `resp[1] == 0x06` (battery status opcode)
/// - `resp[2]` = battery percentage (`0..=100`, or `0xFF` when mouse is asleep/disconnected)
/// - `resp[3]` = charging flag (`0x01` charging, `0x02` full, `0x00` discharging)
#[must_use]
pub fn parse_b4_battery_response(resp: &[u8]) -> Option<PowerState> {
    if resp.len() < 4 || resp[0] != 0xB4 || resp[1] != 0x06 {
        return None;
    }
    let raw_pct = resp[2];
    if raw_pct == 0xFF {
        return Some(PowerState::Unavailable {
            reason: UnavailableReason::DeviceAsleep,
        });
    }
    if raw_pct > 100 {
        return None;
    }
    let charging = match resp[3] {
        1 => ChargingState::Charging,
        2 => ChargingState::Full,
        0 => ChargingState::Discharging,
        _ => ChargingState::Unknown,
    };
    Some(PowerState::Available(BatteryReport::single(
        f32::from(raw_pct),
        charging,
        ReadSource::Vendor("keychron".into()),
    )))
}

/// Parses a Keychron `0x52` feature report battery response (newer Link / Max receivers).
///
/// Layout:
/// - `resp[0] == 0x52`
/// - `resp[2]` = battery percentage (`0..=100`, `0` when sleeping/offline)
/// - `resp[3]` = charging flag (`1` charging, `0` discharging)
#[must_use]
pub fn parse_52_feature_response(resp: &[u8]) -> Option<PowerState> {
    if resp.len() < 4 || resp[0] != 0x52 {
        return None;
    }
    let raw_pct = resp[2];
    if raw_pct == 0 || raw_pct > 100 {
        return None;
    }
    let charging = match resp[3] {
        1 => ChargingState::Charging,
        0 => ChargingState::Discharging,
        _ => ChargingState::Unknown,
    };
    Some(PowerState::Available(BatteryReport::single(
        f32::from(raw_pct),
        charging,
        ReadSource::Vendor("keychron".into()),
    )))
}

#[cfg(all(feature = "hidraw", feature = "sysfs"))]
impl HidVendorDriver for KeychronDriver {
    fn id(&self) -> &'static str {
        "keychron"
    }

    fn matches(&self, info: &HidrawDeviceInfo) -> bool {
        info.vendor_id == VENDOR_ID
    }

    fn read_power<'a>(
        &'a self,
        info: &'a HidrawDeviceInfo,
        dev: &'a HidrawDevice,
    ) -> BoxFuture<'a, Option<PowerState>> {
        Box::pin(async move {
            // Try Report ID 0xB3/0xB4 interrupt exchange first.
            match dev
                .exchange_report(build_b3_battery_request(), 64, Duration::from_millis(250))
                .await
            {
                Ok(resp) => {
                    if let Some(state) = parse_b4_battery_response(&resp) {
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

            // Fallback: try Feature Report 0x51 -> 0x52 (used by some Keychron Link receivers).
            let mut set_req = vec![0u8; 32];
            set_req[0] = 0x51;
            set_req[1] = 0x06;
            if dev.set_feature_report(set_req).await.is_ok() {
                let mut get_req = vec![0u8; 32];
                get_req[0] = 0x52;
                if let Ok(resp) = dev.get_feature_report(get_req).await
                    && let Some(state) = parse_52_feature_response(&resp)
                {
                    return Some(state);
                }
            }

            let _ = info;
            None
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_b4_battery_and_sleep() {
        let state = parse_b4_battery_response(&[0xB4, 0x06, 87, 0x01]).unwrap();
        let report = state.report().unwrap();
        assert_eq!(report.primary_percentage(), Some(87.0));
        assert_eq!(report.cells[0].charging, ChargingState::Charging);

        let asleep = parse_b4_battery_response(&[0xB4, 0x06, 0xFF, 0x00]).unwrap();
        assert_eq!(
            asleep,
            PowerState::Unavailable {
                reason: UnavailableReason::DeviceAsleep
            }
        );
    }

    #[test]
    fn parses_52_feature_response() {
        let state = parse_52_feature_response(&[0x52, 0x00, 64, 0x00]).unwrap();
        assert_eq!(state.report().unwrap().primary_percentage(), Some(64.0));
    }
}

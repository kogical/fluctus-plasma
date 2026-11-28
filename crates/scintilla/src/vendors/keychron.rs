// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! Keychron mice and 2.4 GHz receivers (VID `0x3434`).
//!
//! Over Bluetooth, Keychron mice use the standard GATT Battery Service, which BlueZ reads.
//! Over a receiver or the cable they only answer on a vendor HID collection: usage page
//! `0xFFC1` (1K mice and the Link / Link-KM / Ultra-Link receivers) or `0xFF0A` (4K/8K mice).
//! Requests go out on the interrupt OUT endpoint (as Keychron's software sends them), with a
//! `SET_REPORT(Output)` control transfer as the fallback: depending on its state, a Link-KM
//! answered only one of the two during testing.
//!
//! What is implemented, and how sure we are:
//! - **Paired-device lookup** (`B5 03` → `B6 03`): verified on a Link-KM (`3434:d029`) with an
//!   M6 (`3434:d060`). Lets the mouse show under its own name even when its battery cannot be
//!   read.
//! - **Status** (`B3 06` → `B4 06`, level in byte 20, bit 7 = charging): the Ultra-Link 8K
//!   (`3434:d028`) and wired M5/M6 8K, from `keychron-battery-dkms`. The Link-KM does not answer
//!   it.
//! - **Device info** (feature `51 06`, level in byte 11): wired 1K mice, verified on an M1
//!   by `tailglow`.
//!
//! Not yet implemented: the Launcher's 4K/8K `getPower` exchange for mice paired to a Link-KM.

use std::time::Duration;

use crate::model::{
    BatteryReport, ChargingState, DeviceCategory, PowerState, ReadSource, UnavailableReason,
};
#[cfg(all(feature = "hidraw", feature = "sysfs"))]
use crate::error::Error;
#[cfg(all(feature = "hidraw", feature = "sysfs"))]
use crate::providers::hidraw::{HidrawDevice, OutputPath};
#[cfg(all(feature = "hidraw", feature = "sysfs"))]
use crate::providers::sysfs::HidrawDeviceInfo;
#[cfg(all(feature = "hidraw", feature = "sysfs"))]
use crate::vendors::{BoxFuture, HidVendorDriver, VendorIdentity};

/// Keychron USB Vendor ID.
pub const VENDOR_ID: u16 = 0x3434;
/// Vendor collection of 1K mice and of every Keychron 2.4 GHz receiver.
pub const USAGE_PAGE_MOUSE: u16 = 0xFFC1;
/// Vendor collection of 4K/8K mice.
pub const USAGE_PAGE_MOUSE_4K: u16 = 0xFF0A;

const STATUS_REQUEST_ID: u8 = 0xB3;
const STATUS_RESPONSE_ID: u8 = 0xB4;
const CMD_STATUS: u8 = 0x06;
const STATUS_BATTERY_OFFSET: usize = 20;

const INFO_REQUEST_ID: u8 = 0xB5;
const INFO_RESPONSE_ID: u8 = 0xB6;
const CMD_RECEIVER_STATE: u8 = 0x03;
const RECEIVER_SLOT_OFFSET: usize = 3;
const RECEIVER_SLOT_SIZE: usize = 5;
const RECEIVER_SLOTS: usize = 3;

const FEATURE_ID: u8 = 0x51;
const FEATURE_BATTERY_OFFSET: usize = 11;

const TIMEOUT: Duration = Duration::from_millis(300);
const RETRIES: usize = 2;

/// Keychron product names by USB product ID (mice and the mouse side of receivers).
const MODELS: &[(u16, &str)] = &[
    (0xd035, "Keychron M1"),
    (0xd03b, "Keychron M2"),
    (0xd03d, "Keychron M2 mini"),
    (0xd032, "Keychron M3"),
    (0xd033, "Keychron M3"),
    (0xd036, "Keychron M3 mini"),
    (0xd03e, "Keychron M3 mini"),
    (0xd048, "Keychron M5"),
    (0xd03f, "Keychron M6"),
    (0xd060, "Keychron M6"),
    (0xd064, "Keychron M6"),
    (0xd049, "Keychron M6 8K"),
    (0xd044, "Keychron M7"),
    (0xd056, "Keychron M7 8K"),
    (0x0440, "Keychron Nape Pro"),
    (0x4004, "Keychron Nape Pro"),
];

/// Keychron 2.4 GHz receivers: these never are the device that holds the battery.
const RECEIVERS: &[u16] = &[0xd026, 0xd028, 0xd029, 0xd030, 0xd031, 0xd038, 0xd043];

/// Product name for a Keychron product ID, if it is a known mouse.
#[must_use]
pub fn model_name(product_id: u16) -> Option<&'static str> {
    MODELS
        .iter()
        .find(|&&(pid, _)| pid == product_id)
        .map(|&(_, name)| name)
}

/// Whether `product_id` is one of Keychron's 2.4 GHz receivers.
#[must_use]
pub fn is_receiver(product_id: u16) -> bool {
    RECEIVERS.contains(&product_id)
}

/// A device paired to a Keychron receiver.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct PairedSlot {
    pub vendor_id: u16,
    pub product_id: u16,
    pub connected: bool,
}

/// `B3 06`: status request (64 bytes including the report ID).
#[must_use]
pub fn build_status_request() -> Vec<u8> {
    let mut pkt = vec![0u8; 64];
    pkt[0] = STATUS_REQUEST_ID;
    pkt[1] = CMD_STATUS;
    pkt
}

/// Parses a `B4 06` status reply: byte 20 holds the level (bits 0-6) and charging (bit 7).
#[must_use]
pub fn parse_status_response(resp: &[u8]) -> Option<PowerState> {
    if resp.len() <= STATUS_BATTERY_OFFSET
        || resp[0] != STATUS_RESPONSE_ID
        || resp[1] != CMD_STATUS
    {
        return None;
    }
    let raw = resp[STATUS_BATTERY_OFFSET];
    let level = raw & 0x7F;
    if level > 100 {
        return None;
    }
    let charging = if raw & 0x80 == 0 {
        ChargingState::Discharging
    } else if level == 100 {
        ChargingState::Full
    } else {
        ChargingState::Charging
    };
    Some(PowerState::Available(BatteryReport::single(
        f32::from(level),
        charging,
        ReadSource::Vendor("keychron".into()),
    )))
}

/// `B5 03`: receiver state request (21 bytes including the report ID).
#[must_use]
pub fn build_receiver_query() -> Vec<u8> {
    let mut pkt = vec![0u8; 21];
    pkt[0] = INFO_REQUEST_ID;
    pkt[1] = CMD_RECEIVER_STATE;
    pkt
}

/// Parses a `B6 03` receiver state reply into its paired slots (empty slots are skipped).
///
/// Layout: `[B6, 03, count, (vid LE16, pid LE16, connected) × 3]`.
#[must_use]
pub fn parse_receiver_slots(resp: &[u8]) -> Vec<PairedSlot> {
    if resp.len() < 2
        || !(resp[0] == INFO_RESPONSE_ID || resp[0] == INFO_REQUEST_ID)
        || resp[1] != CMD_RECEIVER_STATE
    {
        return Vec::new();
    }
    (0..RECEIVER_SLOTS)
        .filter_map(|i| {
            let off = RECEIVER_SLOT_OFFSET + i * RECEIVER_SLOT_SIZE;
            let slot = resp.get(off..off + RECEIVER_SLOT_SIZE)?;
            let vendor_id = u16::from_le_bytes([slot[0], slot[1]]);
            let product_id = u16::from_le_bytes([slot[2], slot[3]]);
            (vendor_id != 0 || product_id != 0).then_some(PairedSlot {
                vendor_id,
                product_id,
                connected: slot[4] == 1,
            })
        })
        .collect()
}

/// Parses a wired 1K mouse `51 06` device-info feature report (level in byte 11).
#[must_use]
pub fn parse_feature_info(resp: &[u8]) -> Option<PowerState> {
    if resp.len() <= FEATURE_BATTERY_OFFSET || resp[0] != FEATURE_ID || resp[1] != CMD_STATUS {
        return None;
    }
    let level = resp[FEATURE_BATTERY_OFFSET];
    if level == 0 || level > 100 {
        return None;
    }
    Some(PowerState::Available(BatteryReport::single(
        f32::from(level),
        ChargingState::Unknown,
        ReadSource::Vendor("keychron".into()),
    )))
}

/// The identity of the mouse behind a receiver's paired slots, preferring a connected one.
#[must_use]
pub fn identity_from_slots(slots: &[PairedSlot]) -> Option<(String, bool)> {
    let slot = slots
        .iter()
        .find(|s| s.connected && s.vendor_id == VENDOR_ID)
        .or_else(|| slots.iter().find(|s| s.vendor_id == VENDOR_ID))?;
    let name = model_name(slot.product_id).map_or_else(
        || "Keychron mouse".to_string(),
        std::string::ToString::to_string,
    );
    Some((name, slot.connected))
}

/// Vendor driver for Keychron mice and 2.4 GHz receivers.
#[derive(Debug, Default)]
pub struct KeychronDriver;

#[cfg(all(feature = "hidraw", feature = "sysfs"))]
impl KeychronDriver {
    async fn request(
        dev: &HidrawDevice,
        request: Vec<u8>,
        accept: fn(&[u8]) -> bool,
    ) -> crate::error::Result<Vec<u8>> {
        // The interrupt pipe is what Keychron's own software uses; some receiver states only
        // answer a SET_REPORT control transfer, so that is the fallback.
        let mut last = Err(Error::Timeout(dev.path().display().to_string()));
        for _ in 0..RETRIES {
            for path in [OutputPath::Interrupt, OutputPath::SetReport] {
                last = dev.transact(request.clone(), path, TIMEOUT, accept).await;
                if matches!(last, Ok(_) | Err(Error::PermissionDenied { .. })) {
                    return last;
                }
            }
        }
        last
    }

    async fn receiver_slots(dev: &HidrawDevice) -> crate::error::Result<Vec<PairedSlot>> {
        let resp = Self::request(dev, build_receiver_query(), |r| {
            r.len() > 1
                && (r[0] == INFO_RESPONSE_ID || r[0] == INFO_REQUEST_ID)
                && r[1] == CMD_RECEIVER_STATE
        })
        .await?;
        Ok(parse_receiver_slots(&resp))
    }
}

#[cfg(all(feature = "hidraw", feature = "sysfs"))]
impl HidVendorDriver for KeychronDriver {
    fn id(&self) -> &'static str {
        "keychron"
    }

    fn matches(&self, info: &HidrawDeviceInfo) -> bool {
        // Only the vendor collection: the pointer and keyboard interfaces never answer, and
        // writing to them would be pointless.
        info.vendor_id == VENDOR_ID
            && (info.has_usage_page(USAGE_PAGE_MOUSE) || info.has_usage_page(USAGE_PAGE_MOUSE_4K))
    }

    fn read_power<'a>(
        &'a self,
        info: &'a HidrawDeviceInfo,
        dev: &'a HidrawDevice,
    ) -> BoxFuture<'a, Option<PowerState>> {
        Box::pin(async move {
            match Self::request(dev, build_status_request(), |r| {
                r.len() > 1 && r[0] == STATUS_RESPONSE_ID && r[1] == CMD_STATUS
            })
            .await
            {
                Ok(resp) => {
                    if let Some(state) = parse_status_response(&resp) {
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

            if is_receiver(info.product_id) {
                // A receiver whose mouse is paired but not connected: say so instead of
                // "unsupported".
                if let Ok(slots) = Self::receiver_slots(dev).await
                    && !slots.is_empty()
                    && !slots.iter().any(|s| s.connected)
                {
                    return Some(PowerState::Unavailable {
                        reason: UnavailableReason::DeviceAsleep,
                    });
                }
                return None;
            }

            // Wired 1K mice: SET_FEATURE `51 06`, then GET_FEATURE `51`.
            let mut set = vec![0u8; 21];
            set[0] = FEATURE_ID;
            set[1] = CMD_STATUS;
            if dev.set_feature_report(set).await.is_ok() {
                let mut get = vec![0u8; 21];
                get[0] = FEATURE_ID;
                if let Ok(resp) = dev.get_feature_report(get).await {
                    return parse_feature_info(&resp);
                }
            }
            None
        })
    }

    fn identify<'a>(
        &'a self,
        info: &'a HidrawDeviceInfo,
        dev: &'a HidrawDevice,
    ) -> BoxFuture<'a, Option<VendorIdentity>> {
        Box::pin(async move {
            if !is_receiver(info.product_id) {
                return model_name(info.product_id).map(|name| VendorIdentity {
                    name: name.to_string(),
                    category: DeviceCategory::Mouse,
                    connected: true,
                });
            }
            let slots = Self::receiver_slots(dev).await.ok()?;
            let (name, connected) = identity_from_slots(&slots)?;
            Some(VendorIdentity {
                name,
                category: DeviceCategory::Mouse,
                connected,
            })
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_status_level_and_charging_bit() {
        let mut resp = vec![0u8; 64];
        resp[0] = 0xB4;
        resp[1] = 0x06;
        resp[20] = 87;
        let report = parse_status_response(&resp).unwrap();
        let report = report.report().unwrap();
        assert_eq!(report.primary_percentage(), Some(87.0));
        assert_eq!(report.cells[0].charging, ChargingState::Discharging);

        resp[20] = 0x80 | 42;
        let report = parse_status_response(&resp).unwrap();
        assert_eq!(report.report().unwrap().primary_percentage(), Some(42.0));
        assert_eq!(report.report().unwrap().cells[0].charging, ChargingState::Charging);

        resp[20] = 0x7F;
        assert!(parse_status_response(&resp).is_none(), "levels above 100 are refused");
        assert!(parse_status_response(&[0xB4, 0x06, 50]).is_none(), "short replies are refused");
    }

    #[test]
    fn parses_receiver_slots_captured_from_link_km() {
        // Reply captured from a Keychron Link-KM (3434:d029) with an M6 (3434:d060) connected.
        let resp = hex_bytes("b60301343460d00100000000000000000000000000");
        let slots = parse_receiver_slots(&resp);
        assert_eq!(
            slots,
            vec![PairedSlot {
                vendor_id: 0x3434,
                product_id: 0xd060,
                connected: true
            }]
        );
        assert_eq!(
            identity_from_slots(&slots),
            Some(("Keychron M6".to_string(), true))
        );
    }

    #[test]
    fn unknown_paired_mouse_gets_a_generic_name() {
        let slots = [PairedSlot {
            vendor_id: VENDOR_ID,
            product_id: 0xdfff,
            connected: false,
        }];
        assert_eq!(
            identity_from_slots(&slots),
            Some(("Keychron mouse".to_string(), false))
        );
        assert!(parse_receiver_slots(&[0xB6, 0x02, 0x00]).is_empty());
    }

    #[test]
    fn parses_wired_feature_info() {
        // `51 06` reply from a wired M1 (tailglow capture): byte 11 = 0x64 (100 %).
        let resp = hex_bytes("51060100343435d000010064020000000000000000");
        let state = parse_feature_info(&resp).unwrap();
        assert_eq!(state.report().unwrap().primary_percentage(), Some(100.0));
    }

    #[test]
    fn knows_receivers_and_models() {
        assert!(is_receiver(0xd029));
        assert!(!is_receiver(0xd060));
        assert_eq!(model_name(0xd060), Some("Keychron M6"));
        assert_eq!(model_name(0x1234), None);
    }

    fn hex_bytes(s: &str) -> Vec<u8> {
        (0..s.len())
            .step_by(2)
            .map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap())
            .collect()
    }
}

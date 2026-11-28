// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! C ABI for embedding `scintilla` inside C/C++ hosts such as Fluctus Battery.
//!
//! All functions are thread-safe to call from any single worker thread owning a
//! [`ScintillaClient`] handle. Memory returned by [`scintilla_client_discover_json`]
//! must be released with [`scintilla_string_free`].

use std::ffi::{CString, c_char};
use std::fmt::Write as _;
use std::ptr;

use crate::manager::{Manager, ManagerConfig};
use crate::model::{
    BatteryCell, CellRole, ChargingState, CoarseLevel, Device, DeviceCategory, PowerState,
    ReadSource, Transport, UnavailableReason,
};

/// Opaque FFI handle owning a dedicated single-threaded Tokio runtime and [`Manager`].
pub struct ScintillaClient {
    runtime: tokio::runtime::Runtime,
    manager: Manager,
}

/// Creates a new `scintilla` client handle.
///
/// Returns `NULL` if the internal Tokio runtime or manager cannot be initialized.
#[unsafe(no_mangle)]
pub extern "C" fn scintilla_client_new(
    include_stubs: bool,
    enable_vendor_probing: bool,
) -> *mut ScintillaClient {
    let Ok(runtime) = tokio::runtime::Builder::new_current_thread()
        .enable_all()
        .build()
    else {
        return ptr::null_mut();
    };

    let config = ManagerConfig {
        include_stubs,
        enable_vendor_probing,
    };

    let Ok(manager) = runtime.block_on(Manager::with_config(config)) else {
        return ptr::null_mut();
    };

    Box::into_raw(Box::new(ScintillaClient { runtime, manager }))
}

/// Destroys a client handle created by [`scintilla_client_new`]. Passing `NULL` is a no-op.
///
/// # Safety
/// `client` must be `NULL` or a live pointer previously returned by [`scintilla_client_new`]
/// that has not already been freed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn scintilla_client_free(client: *mut ScintillaClient) {
    if !client.is_null() {
        // SAFETY: caller guarantees `client` came from `Box::into_raw` and is freed once.
        unsafe {
            drop(Box::from_raw(client));
        }
    }
}

/// Performs a oneshot discovery across all enabled providers and returns a UTF-8 JSON array
/// describing all discovered devices.
///
/// The returned string must be freed with [`scintilla_string_free`].
/// Returns `NULL` if `client` is `NULL` or discovery fails.
///
/// # Safety
/// `client` must be `NULL` or a valid pointer returned by [`scintilla_client_new`], and must
/// not be accessed concurrently from multiple threads without external synchronization.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn scintilla_client_discover_json(
    client: *mut ScintillaClient,
) -> *mut c_char {
    if client.is_null() {
        return ptr::null_mut();
    }

    // SAFETY: caller guarantees `client` is valid and exclusively accessed for this call.
    let client_ref = unsafe { &*client };
    let Ok(devices) = client_ref.runtime.block_on(client_ref.manager.devices()) else {
        return ptr::null_mut();
    };

    let json = serialize_devices_json(&devices);
    match CString::new(json) {
        Ok(cstr) => cstr.into_raw(),
        Err(_) => ptr::null_mut(),
    }
}

/// Frees a NUL-terminated string returned by [`scintilla_client_discover_json`].
/// Passing `NULL` is a no-op.
///
/// # Safety
/// `ptr` must be `NULL` or a pointer previously returned by [`scintilla_client_discover_json`]
/// that has not already been freed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn scintilla_string_free(ptr: *mut c_char) {
    if !ptr.is_null() {
        // SAFETY: caller guarantees `ptr` came from `CString::into_raw` and is freed once.
        unsafe {
            drop(CString::from_raw(ptr));
        }
    }
}

fn push_json_str(out: &mut String, s: &str) {
    out.push('"');
    for ch in s.chars() {
        match ch {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            c if (c as u32) < 0x20 => {
                let _ = write!(out, "\\u{:04x}", c as u32);
            }
            c => out.push(c),
        }
    }
    out.push('"');
}

fn push_opt_str(out: &mut String, opt: Option<&str>) {
    match opt {
        Some(s) => push_json_str(out, s),
        None => out.push_str("null"),
    }
}

const fn category_str(c: DeviceCategory) -> &'static str {
    match c {
        DeviceCategory::Laptop => "laptop",
        DeviceCategory::LinePower => "line_power",
        DeviceCategory::Ups => "ups",
        DeviceCategory::Mouse => "mouse",
        DeviceCategory::Keyboard => "keyboard",
        DeviceCategory::Headset => "headset",
        DeviceCategory::Earbuds => "earbuds",
        DeviceCategory::Headphones => "headphones",
        DeviceCategory::Speaker => "speaker",
        DeviceCategory::Controller => "controller",
        DeviceCategory::Tablet => "tablet",
        DeviceCategory::Phone => "phone",
        DeviceCategory::Wearable => "wearable",
        DeviceCategory::Stylus => "stylus",
        DeviceCategory::Unknown => "unknown",
    }
}

const fn transport_str(t: Transport) -> &'static str {
    match t {
        Transport::Internal => "internal",
        Transport::Usb => "usb",
        Transport::WirelessDongle => "wireless_dongle",
        Transport::BluetoothClassic => "bluetooth_classic",
        Transport::Ble => "ble",
        Transport::Unknown => "unknown",
    }
}

const fn cell_role_str(r: CellRole) -> &'static str {
    match r {
        CellRole::Primary => "primary",
        CellRole::Left => "left",
        CellRole::Right => "right",
        CellRole::Case => "case",
        CellRole::Dock => "dock",
        CellRole::Spare => "spare",
    }
}

const fn coarse_str(c: CoarseLevel) -> &'static str {
    match c {
        CoarseLevel::Critical => "critical",
        CoarseLevel::Low => "low",
        CoarseLevel::Normal => "normal",
        CoarseLevel::High => "high",
        CoarseLevel::Full => "full",
    }
}

const fn charging_str(c: ChargingState) -> &'static str {
    match c {
        ChargingState::Charging => "charging",
        ChargingState::Discharging => "discharging",
        ChargingState::Full => "full",
        ChargingState::NotCharging => "not_charging",
        ChargingState::Unknown => "unknown",
    }
}

fn source_str(s: &ReadSource) -> String {
    match s {
        ReadSource::Sysfs => "sysfs".to_string(),
        ReadSource::UPower => "upower".to_string(),
        ReadSource::BluezBattery1 => "bluez".to_string(),
        ReadSource::BleGattBas => "ble_gatt".to_string(),
        ReadSource::HidPowerPage => "hid_power".to_string(),
        ReadSource::Vendor(name) => format!("vendor:{name}"),
    }
}

const fn unavailable_reason_str(r: &UnavailableReason) -> &'static str {
    match r {
        UnavailableReason::NoBattery => "no_battery",
        UnavailableReason::UnsupportedProtocol => "unsupported_protocol",
        UnavailableReason::PermissionDenied { .. } => "permission_denied",
        UnavailableReason::StackFeatureDisabled(_) => "stack_feature_disabled",
        UnavailableReason::DeviceAsleep => "device_asleep",
        UnavailableReason::ReadError(_) => "read_error",
    }
}

fn push_cell_json(out: &mut String, cell: &BatteryCell) {
    out.push('{');
    out.push_str("\"role\":");
    push_json_str(out, cell_role_str(cell.role));
    out.push_str(",\"percentage\":");
    match cell.percentage {
        Some(p) if p.is_finite() => {
            let _ = write!(out, "{p:.1}");
        }
        _ => out.push_str("null"),
    }
    out.push_str(",\"coarse\":");
    push_opt_str(out, cell.coarse.map(coarse_str));
    out.push_str(",\"charging\":");
    push_json_str(out, charging_str(cell.charging));
    out.push_str(",\"voltage_mv\":");
    match cell.voltage_mv {
        Some(v) => {
            let _ = write!(out, "{v}");
        }
        None => out.push_str("null"),
    }
    out.push('}');
}

/// Serializes a slice of [`Device`] records into a compact JSON array.
#[must_use]
pub fn serialize_devices_json(devices: &[Device]) -> String {
    let mut out = String::with_capacity(devices.len() * 192 + 2);
    out.push('[');
    for (idx, dev) in devices.iter().enumerate() {
        if idx > 0 {
            out.push(',');
        }
        out.push('{');
        out.push_str("\"id\":");
        push_json_str(&mut out, &dev.id.0);
        out.push_str(",\"name\":");
        push_json_str(&mut out, &dev.name);
        out.push_str(",\"vendor\":");
        push_opt_str(&mut out, dev.vendor.as_deref());
        out.push_str(",\"model\":");
        push_opt_str(&mut out, dev.model.as_deref());
        out.push_str(",\"category\":");
        push_json_str(&mut out, category_str(dev.category));
        out.push_str(",\"transport\":");
        push_json_str(&mut out, transport_str(dev.transport));

        match &dev.power {
            PowerState::Available(report) => {
                out.push_str(",\"available\":true,\"source\":");
                let src = source_str(&report.source);
                push_json_str(&mut out, &src);
                out.push_str(",\"unavailable_reason\":null,\"cells\":[");
                for (ci, cell) in report.cells.iter().enumerate() {
                    if ci > 0 {
                        out.push(',');
                    }
                    push_cell_json(&mut out, cell);
                }
                out.push(']');
            }
            PowerState::Unavailable { reason } => {
                out.push_str(",\"available\":false,\"source\":null,\"unavailable_reason\":");
                push_json_str(&mut out, unavailable_reason_str(reason));
                out.push_str(",\"cells\":[]");
            }
        }
        out.push('}');
    }
    out.push(']');
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::model::{BatteryReport, DeviceId};
    use std::time::SystemTime;

    #[test]
    fn serializes_multi_cell_and_stub_devices() {
        let earbuds = Device {
            id: DeviceId("bt:AA:BB:CC:DD:EE:FF".to_string()),
            addresses: vec![],
            name: "Pro \"Buds\" \\ 2".to_string(),
            vendor: Some("Acme".to_string()),
            model: None,
            vendor_id: None,
            product_id: None,
            category: DeviceCategory::Earbuds,
            transport: Transport::Ble,
            power: PowerState::Available(BatteryReport {
                cells: vec![
                    BatteryCell {
                        role: CellRole::Left,
                        percentage: Some(82.0),
                        coarse: None,
                        charging: ChargingState::Discharging,
                        voltage_mv: None,
                    },
                    BatteryCell {
                        role: CellRole::Right,
                        percentage: Some(78.5),
                        coarse: None,
                        charging: ChargingState::Discharging,
                        voltage_mv: None,
                    },
                    BatteryCell {
                        role: CellRole::Case,
                        percentage: Some(100.0),
                        coarse: None,
                        charging: ChargingState::Full,
                        voltage_mv: Some(4200),
                    },
                ],
                source: ReadSource::BleGattBas,
                updated_at: SystemTime::UNIX_EPOCH,
            }),
        };

        let stub = Device {
            id: DeviceId("usb:1234:5678".to_string()),
            addresses: vec![],
            name: "Wired Keyboard".to_string(),
            vendor: None,
            model: None,
            vendor_id: Some(0x1234),
            product_id: Some(0x5678),
            category: DeviceCategory::Keyboard,
            transport: Transport::Usb,
            power: PowerState::Unavailable {
                reason: UnavailableReason::NoBattery,
            },
        };

        let json = serialize_devices_json(&[earbuds, stub]);
        assert!(json.contains(r#""name":"Pro \"Buds\" \\ 2""#));
        assert!(json.contains(r#""category":"earbuds""#));
        assert!(json.contains(r#""role":"left","percentage":82.0"#));
        assert!(json.contains(r#""role":"case","percentage":100.0,"coarse":null,"charging":"full","voltage_mv":4200"#));
        assert!(json.contains(r#""available":false,"source":null,"unavailable_reason":"no_battery","cells":[]"#));
    }

    #[test]
    fn null_pointers_are_safe() {
        // SAFETY: passing null is explicitly documented as a no-op.
        unsafe {
            scintilla_client_free(ptr::null_mut());
            scintilla_string_free(ptr::null_mut());
            assert!(scintilla_client_discover_json(ptr::null_mut()).is_null());
        }
    }
}

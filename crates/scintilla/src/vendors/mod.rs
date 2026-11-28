// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! Modular proprietary vendor drivers ("Hall of Shame").
//!
//! Each vendor lives in its own self-contained, feature-gated module. Adding a
//! new vendor requires implementing either [`HidVendorDriver`] or
//! [`BluetoothVendorDriver`] and registering it in [`hid_drivers`] /
//! [`bluetooth_drivers`].

#[cfg(feature = "vendor-apple-airpods")]
pub mod apple_airpods;
#[cfg(feature = "vendor-bose")]
pub mod bose;
#[cfg(feature = "vendor-corsair")]
pub mod corsair;
#[cfg(feature = "vendor-keychron")]
pub mod keychron;
#[cfg(feature = "vendor-logitech")]
pub mod logitech;
#[cfg(feature = "vendor-razer")]
pub mod razer;
#[cfg(feature = "vendor-samsung-buds")]
pub mod samsung_buds;
#[cfg(feature = "vendor-sony-audio")]
pub mod sony_audio;
#[cfg(feature = "vendor-steelseries")]
pub mod steelseries;

use std::future::Future;
use std::pin::Pin;

#[cfg(any(feature = "bluez", all(feature = "hidraw", feature = "sysfs")))]
use crate::model::PowerState;

/// Boxed async future alias for dyn-compatible driver traits.
pub type BoxFuture<'a, T> = Pin<Box<dyn Future<Output = T> + Send + 'a>>;

/// Async driver for a proprietary USB/2.4 GHz HID peripheral accessed via `/dev/hidraw*`.
#[cfg(all(feature = "hidraw", feature = "sysfs"))]
pub trait HidVendorDriver: Send + Sync {
    /// Human-readable driver identifier (e.g. `"keychron"`).
    fn id(&self) -> &'static str;

    /// Returns `true` if this driver handles the given `hidraw` device.
    fn matches(&self, info: &crate::providers::sysfs::HidrawDeviceInfo) -> bool;

    /// Queries battery telemetry asynchronously over `hidraw`.
    fn read_power<'a>(
        &'a self,
        info: &'a crate::providers::sysfs::HidrawDeviceInfo,
        dev: &'a crate::providers::hidraw::HidrawDevice,
    ) -> BoxFuture<'a, Option<PowerState>>;
}

/// Async driver for a proprietary Bluetooth device (passive BLE advertisements or RFCOMM).
#[cfg(feature = "bluez")]
pub trait BluetoothVendorDriver: Send + Sync {
    /// Human-readable driver identifier (e.g. `"apple-airpods"`).
    fn id(&self) -> &'static str;

    /// Returns `true` if this driver applies to the connected BlueZ device snapshot.
    fn matches(&self, snap: &crate::providers::bluez::BluezDeviceSnapshot) -> bool;

    /// Queries or decodes battery telemetry asynchronously.
    fn read_power<'a>(
        &'a self,
        snap: &'a crate::providers::bluez::BluezDeviceSnapshot,
    ) -> BoxFuture<'a, Option<PowerState>>;
}

/// Returns all compiled-in HID vendor drivers.
#[cfg(all(feature = "hidraw", feature = "sysfs"))]
#[must_use]
pub fn hid_drivers() -> Vec<Box<dyn HidVendorDriver>> {
    vec![
        #[cfg(feature = "vendor-keychron")]
        Box::new(keychron::KeychronDriver),
        #[cfg(feature = "vendor-logitech")]
        Box::new(logitech::LogitechHidppDriver),
        #[cfg(feature = "vendor-razer")]
        Box::new(razer::RazerDriver),
        #[cfg(feature = "vendor-corsair")]
        Box::new(corsair::CorsairDriver),
        #[cfg(feature = "vendor-steelseries")]
        Box::new(steelseries::SteelSeriesDriver),
    ]
}

/// Returns all compiled-in Bluetooth vendor drivers.
#[cfg(feature = "bluez")]
#[must_use]
pub fn bluetooth_drivers() -> Vec<Box<dyn BluetoothVendorDriver>> {
    vec![
        #[cfg(feature = "vendor-apple-airpods")]
        Box::new(apple_airpods::AppleAirPodsDriver),
        #[cfg(feature = "vendor-sony-audio")]
        Box::new(sony_audio::SonyAudioDriver),
        #[cfg(feature = "vendor-samsung-buds")]
        Box::new(samsung_buds::SamsungBudsDriver),
        #[cfg(feature = "vendor-bose")]
        Box::new(bose::BoseBmapDriver),
    ]
}

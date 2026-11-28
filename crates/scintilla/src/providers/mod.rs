// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! Standardized OS and protocol providers.

#[cfg(feature = "bluez")]
pub mod bluez;
#[cfg(feature = "hidraw")]
pub mod hidraw;
#[cfg(feature = "rfcomm")]
pub mod rfcomm;
#[cfg(feature = "sysfs")]
pub mod sysfs;
#[cfg(feature = "upower")]
pub mod upower;

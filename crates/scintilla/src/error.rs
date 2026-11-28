// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! Error types for `scintilla`.

use std::path::PathBuf;

/// Result alias using [`Error`].
pub type Result<T> = std::result::Result<T, Error>;

/// Errors returned by `scintilla` discovery, probing, and lookup APIs.
#[derive(Debug, thiserror::Error)]
pub enum Error {
    /// Failed to parse a string into a [`crate::DeviceAddress`].
    #[error("invalid device address `{0}`")]
    InvalidAddress(String),

    /// Permission denied when opening a hardware interface (e.g. `/dev/hidraw*`).
    #[error("permission denied accessing `{path}` (missing udev uaccess rule?)")]
    PermissionDenied { path: PathBuf },

    /// Underlying filesystem or device I/O error.
    #[error("I/O error: {0}")]
    Io(#[from] std::io::Error),

    /// D-Bus communication error (UPower or BlueZ).
    #[cfg(any(feature = "upower", feature = "bluez"))]
    #[error("D-Bus error: {0}")]
    Dbus(#[from] zbus::Error),

    /// A protocol response was malformed or unexpected.
    #[error("protocol error from {source_name}: {detail}")]
    Protocol {
        source_name: &'static str,
        detail: String,
    },

    /// Operation timed out waiting for device telemetry.
    #[error("timed out waiting for response from {0}")]
    Timeout(String),

    /// Requested backend or platform feature is not enabled or supported on this OS.
    #[error("unsupported backend or platform feature: {0}")]
    Unsupported(&'static str),
}

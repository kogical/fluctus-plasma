// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! **scintilla** — async connected-device discovery and battery level reading.
//!
//! `scintilla` discovers connected peripherals across standard Linux interfaces
//! (`sysfs`, `UPower`, `BlueZ`, BLE GATT Battery Service `0x180F`, USB HID Power
//! Page `0x84`/`0x85`) as well as opt-in proprietary vendor modules, returning
//! both live multi-cell battery telemetry and structured [`PowerState::Unavailable`]
//! stubs for devices whose battery cannot be read.
//!
//! All I/O is fully asynchronous on Tokio — there are no synchronous wrappers.
//!
//! # Quick Start
//!
//! ```no_run
//! use std::time::Duration;
//! use scintilla::{Manager, PowerState};
//!
//! # async fn example() -> scintilla::Result<()> {
//! let manager = Manager::new().await?;
//!
//! // Oneshot discovery (includes stubs for unreadable devices)
//! for dev in manager.devices().await? {
//!     match &dev.power {
//!         PowerState::Available(report) => {
//!             println!("{}: {:?}%", dev.name, report.primary_percentage());
//!         }
//!         PowerState::Unavailable { reason } => {
//!             println!("{}: connected (no battery read: {:?})", dev.name, reason);
//!         }
//!     }
//! }
//!
//! // Reactive watch (event-driven when supported, polling otherwise; stops on drop)
//! let mut watch = manager
//!     .watch_id("AA:BB:CC:DD:EE:FF", scintilla::WatchMode::Auto { poll_interval: Duration::from_secs(60) })
//!     .await?;
//! println!("Using polling: {}", watch.is_polling());
//! while let Some(event) = watch.recv().await {
//!     println!("{event:?}");
//! }
//! # Ok(())
//! # }
//! ```

pub mod error;
pub mod manager;
pub mod model;
pub mod providers;
pub mod vendors;
pub mod watch;

mod correlate;

pub use error::{Error, Result};
pub use manager::{Manager, ManagerConfig};
pub use model::{
    BatteryCell, BatteryReport, CellRole, ChargingState, CoarseLevel, Device, DeviceAddress,
    DeviceCategory, DeviceEvent, DeviceId, PowerState, ReadSource, Transport, UnavailableReason,
};
pub use watch::{BatteryWatch, WatchEvent, WatchMode, WatchStrategy};

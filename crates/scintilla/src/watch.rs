// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! Reactive battery watch handles with RAII cancellation on drop.
//!
//! When a caller requests a [`BatteryWatch`], `scintilla` automatically determines
//! whether the target device can be observed via native OS events (D-Bus
//! `PropertiesChanged` on UPower / BlueZ) or requires periodic polling (e.g.
//! `hidraw` vendor devices, RFCOMM audio, or `sysfs`-only nodes).
//!
//! Dropping the [`BatteryWatch`] handle immediately aborts its background Tokio
//! task, stopping all event subscriptions and polling for that device.

use std::time::Duration;

use tokio::sync::mpsc;
use tokio::task::AbortHandle;

use crate::error::Error;
use crate::model::{Device, DeviceAddress, PowerState};

/// Caller preference when requesting a [`BatteryWatch`].
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum WatchMode {
    /// Automatically use native OS push events when available, falling back to
    /// polling at `poll_interval` only when the device has no event interface.
    Auto { poll_interval: Duration },
    /// Only allow native event-driven updates (UPower / BlueZ D-Bus).
    /// Fails with [`Error::Unsupported`] if the device requires polling.
    EventOnly,
    /// Force periodic polling at the given interval even if an event interface exists.
    PollingOnly(Duration),
}

/// Strategy actually selected by `scintilla` for an active [`BatteryWatch`].
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum WatchStrategy {
    /// Backed by native OS push signals (UPower or BlueZ D-Bus `PropertiesChanged`).
    /// The caller's `poll_interval` is not used.
    EventDriven,
    /// Device has no push-event interface (or [`WatchMode::PollingOnly`] was requested);
    /// `scintilla` polls at this interval.
    Polling(Duration),
}

/// Event delivered over a [`BatteryWatch`] channel.
#[derive(Debug)]
pub enum WatchEvent {
    /// Initial snapshot or subsequent change in the device's [`PowerState`].
    Updated {
        device: Box<Device>,
        power: PowerState,
    },
    /// A non-fatal read error occurred while refreshing this device.
    Error(Error),
    /// The watched device is no longer connected.
    Disconnected(DeviceAddress),
}

/// RAII handle for receiving reactive battery updates for a single device.
///
/// Dropping this handle immediately cancels the underlying event listener or
/// polling task so no background CPU or radio wakeups continue.
pub struct BatteryWatch {
    rx: mpsc::Receiver<WatchEvent>,
    abort_handle: AbortHandle,
    strategy: WatchStrategy,
}

impl BatteryWatch {
    pub(crate) fn new(
        rx: mpsc::Receiver<WatchEvent>,
        abort_handle: AbortHandle,
        strategy: WatchStrategy,
    ) -> Self {
        Self {
            rx,
            abort_handle,
            strategy,
        }
    }

    /// Returns whether this watch is natively event-driven or falling back to polling.
    #[must_use]
    pub fn strategy(&self) -> WatchStrategy {
        self.strategy
    }

    /// Returns `true` if this watch ended up using periodic polling rather than native OS events.
    #[must_use]
    pub fn is_polling(&self) -> bool {
        matches!(self.strategy, WatchStrategy::Polling(_))
    }

    /// Waits asynchronously for the next [`WatchEvent`].
    ///
    /// Returns `None` if the background watcher terminated.
    pub async fn recv(&mut self) -> Option<WatchEvent> {
        self.rx.recv().await
    }

    /// Non-blocking check for a queued [`WatchEvent`].
    pub fn try_recv(&mut self) -> Result<WatchEvent, mpsc::error::TryRecvError> {
        self.rx.try_recv()
    }
}

impl Drop for BatteryWatch {
    fn drop(&mut self) {
        self.abort_handle.abort();
    }
}

#[cfg(test)]
mod tests {
    use std::sync::Arc;
    use std::sync::atomic::{AtomicBool, Ordering};

    use super::*;

    #[tokio::test]
    async fn dropping_watch_aborts_background_task() {
        let (_tx, rx) = mpsc::channel(4);
        let dropped = Arc::new(AtomicBool::new(false));
        let dropped_flag = Arc::clone(&dropped);

        struct Guard(Arc<AtomicBool>);
        impl Drop for Guard {
            fn drop(&mut self) {
                self.0.store(true, Ordering::SeqCst);
            }
        }

        let (started_tx, started_rx) = tokio::sync::oneshot::channel();
        let handle = tokio::spawn(async move {
            let _guard = Guard(dropped_flag);
            let _ = started_tx.send(());
            std::future::pending::<()>().await;
        });

        started_rx.await.unwrap();
        let watch = BatteryWatch::new(rx, handle.abort_handle(), WatchStrategy::EventDriven);
        assert_eq!(watch.strategy(), WatchStrategy::EventDriven);
        assert!(!watch.is_polling());
        drop(watch);

        let err = handle.await.unwrap_err();
        assert!(err.is_cancelled());
        assert!(dropped.load(Ordering::SeqCst));
    }
}

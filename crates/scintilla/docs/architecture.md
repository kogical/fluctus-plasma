# Architecture

`scintilla` is structured around four design principles:

1. **Fully async (Tokio only):** Every public and internal I/O path is `async fn`. There are no synchronous wrappers. Blocking Linux syscalls (`hidraw` ioctls and `AF_BLUETOOTH` RFCOMM sockets) are isolated inside `tokio::task::spawn_blocking`.
2. **Stub-first discovery:** Devices are discovered independently of whether their battery can be read. Every connected device is returned with either `PowerState::Available(BatteryReport)` or `PowerState::Unavailable { reason }`.
3. **Standards before quirks:** Kernel `sysfs`, `UPower`, `BlueZ`, direct BLE GATT `0x180F`, and standard USB HID Usage Page `0x84`/`0x85` always take precedence over proprietary vendor drivers.
4. **RAII reactive watches:** `BatteryWatch` owns an `AbortHandle` for its background Tokio task. Dropping the handle immediately cancels all D-Bus subscriptions or polling loops.

---

## Discovery & Correlation Pipeline

A single physical peripheral frequently appears in multiple Linux subsystems simultaneously (for example, a Bluetooth mouse can appear in BlueZ `Device1`, BlueZ `Battery1`, UPower, `/sys/class/power_supply/hid-*-battery`, and `/dev/hidraw*`).

`scintilla` handles this in two stages:

1. **Raw observation collection (`src/manager.rs`):** Each enabled provider emits `RawObservation` records containing all known `DeviceAddress` aliases and whatever `PowerState` it could read.
2. **Identity graph merging (`src/correlate.rs`):** Observations sharing any strong identity (normalized Bluetooth MAC, `HidrawPath`, `SysfsPath`, `UPowerPath`, or USB `(vid, pid, serial/bus_addr)`) are merged transitively. When multiple battery reports exist for the same physical device, `BatteryReport::quality_score()` selects the richest one:
   - Multi-cell reports receive a `+40` bonus (so a 2-cell BLE GATT ZMK keyboard or 3-cell AirPods report beats a single-cell UPower/BlueZ summary).
   - Among single-cell reports, priority is: `BleGattBas` > `Sysfs` > `UPower` > `BluezBattery1` > `HidPowerPage` > `Vendor`.
   - Among `Unavailable` stubs, more actionable reasons (`PermissionDenied`, `DeviceAsleep`, `StackFeatureDisabled`) beat generic `UnsupportedProtocol`.

---

## `BatteryWatch` Strategy Selection

When calling `Manager::watch_device(address, mode)`:

- **`WatchMode::Auto { poll_interval }`:** Uses `WatchStrategy::EventDriven` if the device is backed by `UPower`, `BluezBattery1`, or `BleGattBas` over D-Bus (`org.freedesktop.DBus.Properties.PropertiesChanged` via `zbus::MessageStream`, ignoring `poll_interval`). Otherwise falls back to `WatchStrategy::Polling(poll_interval)` and only emits `WatchEvent::Updated` when the `PowerState` changes.
- **`WatchMode::EventOnly`:** Requires a native event-driven source and returns `Err(Error::Unsupported(...))` if the device requires polling.
- **`WatchMode::PollingOnly(interval)`:** Forces periodic polling even if an event interface exists.

Call `watch.is_polling()` or `watch.strategy()` to inspect what was actually chosen. Dropping `BatteryWatch` immediately aborts the spawned Tokio task, freeing the event listener or polling loop.

---

## Linux Permissions (`udev` Rules)

- `sysfs`, `UPower`, and `BlueZ` work **unprivileged** out of the box.
- Direct `/dev/hidraw*` access (for standard HID Power Page `0x84`/`0x85` and USB/2.4 GHz vendor drivers) requires read/write access to the hidraw node. If permission is denied, `scintilla` does not drop the device — it returns `PowerState::Unavailable { reason: UnavailableReason::PermissionDenied { path } }`.

To grant unprivileged access to your local desktop session, install `udev/70-scintilla.rules`:

```bash
sudo cp crates/scintilla/udev/70-scintilla.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

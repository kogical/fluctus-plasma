# Adding a Vendor Module

Each proprietary vendor driver lives in its own self-contained file under `src/vendors/` behind a Cargo feature flag.

## Steps

1. **Create `src/vendors/<vendor>.rs`** with SPDX headers (`Apache-2.0 OR MIT`).
2. **Keep protocol framing pure and unit-tested:** Put packet builders and response parsers in plain functions so `cargo test` verifies them without physical hardware.
3. **Implement the appropriate trait:**
   - `HidVendorDriver` for USB / 2.4 GHz dongle devices accessed via `HidrawDevice`.
   - `BluetoothVendorDriver` for passive BLE `ManufacturerData` parsers or Classic Bluetooth RFCOMM/SPP devices.
4. **Never issue destructive or write-only configuration opcodes.** Battery drivers must remain strictly read-only.
5. **Handle `Error::PermissionDenied` explicitly** by returning `Some(PowerState::Unavailable { reason: UnavailableReason::PermissionDenied { path } })` so UIs can surface actionable permission hints.
6. **Register the module:**
   - Add `vendor-<name>` to `Cargo.toml` and include it in the `vendors` meta-feature.
   - Export `pub mod <vendor>;` and register it in `hid_drivers()` or `bluetooth_drivers()` in `src/vendors/mod.rs`.
   - If the vendor uses `hidraw`, add its USB Vendor ID to `udev/70-scintilla.rules`.
   - Document the protocol and why it's needed in `HALL-OF-SHAME.md`.

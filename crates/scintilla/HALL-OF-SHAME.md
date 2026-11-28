# Hall of Shame — Non-Standard Battery Protocols

Reporting a battery level requires an unsigned integer (`0..=100`) and a charging state. Open, royalty-free standards for this have existed for decades:

- **USB HID Usage Page `0x85` (Battery System)** — standardized in 1997.
- **Bluetooth GATT Battery Service (`0x180F` / `0x2A19`)** — standardized in 2011, including native support for multiple cells.
- **Bluetooth HFP `AT+BIEV`** — standardized by the Bluetooth SIG.

Every manufacturer listed below chose to ignore those standards on at least part of their product line, forcing Linux userspace tools to maintain reverse-engineered protocol drivers under `src/vendors/`.

> **Disclaimer:** Most vendor drivers in `scintilla` are best-effort implementations based on community reverse-engineering and public documentation, not tested on physical hardware. PRs with real-world fixes and captures are always welcome.

---

## Manufacturers

### Razer (VID `0x1532`)
- **What they do:** Require 90-byte USB HID Feature Reports (`Report ID 0x00`) containing a status byte, per-generation transaction ID (`0x1F` / `0x3F`), command class (`0x07`), command ID (`0x80` battery / `0x84` charging), and a manual XOR checksum of bytes `2..=87` in byte `88`. Battery values are scaled `0..=255` instead of `0..=100`, and HyperSpeed dongles report sleeping peripherals as `0`.
- **What they could have done:** Exposed USB HID Usage Page `0x85`, Usage `0x66` (`RemainingCapacity`).
- **Driver:** `src/vendors/razer.rs` (based on `OpenRazer`).

### Apple — Audio Division (AirPods / Beats, Company ID `0x004C`)
- **What they do:** Omit Bluetooth GATT Battery Service (`0x180F`) entirely. Left, Right, and Case battery levels are instead broadcast inside proprietary BLE Continuity advertisements (`Company ID 0x004C`, type `0x07`) behind rotating MAC addresses, or over L2CAP / Apple Accessory Protocol (AACP).
- **What they could have done:** Exposed standard GATT `0x180F` with three `0x2A19` characteristics.
- **Driver:** `src/vendors/apple_airpods.rs` (passive BLE `ManufacturerData` parser).

### Corsair (VID `0x1B1C`)
- **What they do:** Custom 64-byte and 128-byte vendor HID protocols (`0xC9 0x64` on Void/Virtuoso headsets, `0x0E 0x50` on Slipstream dongles) spanning multiple incompatible protocol generations. Write commands (`0x07`) can factory-reset devices if malformed; `scintilla` strictly uses read-only opcodes.
- **What they could have done:** Standard USB HID Usage Page `0x85`.
- **Driver:** `src/vendors/corsair.rs` (based on `ckb-next` and `HeadsetControl`).

### SteelSeries (VID `0x1038`)
- **What they do:** Per-generation proprietary `hidraw` queries (`[0x06, 0x18]` on older Arctis, `[0x06, 0xB0]` on Nova), mixing percentage responses, `0..=4` coarse bar enums, and base-station multiplexed dual batteries depending on SKU.
- **What they could have done:** Standard USB HID Usage Page `0x85` (or upstreaming all SKUs into `hid-steelseries`).
- **Driver:** `src/vendors/steelseries.rs` (based on `HeadsetControl` and `hid-steelseries`).

### Keychron — 2.4 GHz & Wired Mode (VID `0x3434`)
- **What they do:** Over Bluetooth, Keychron peripherals generally comply with GATT `0x180F`. Over USB-C or a Keychron receiver, however, they switch to undocumented vendor HID reports on a vendor collection (`0xFFC1`, or `0xFF0A` for 4K/8K mice), with several protocol generations: `B3 06` → `B4 06` (level in byte 20) on the Ultra-Link 8K, feature report `51 06` on wired 1K mice, and something else again on the Link-KM. The receivers ignore their interrupt OUT endpoint; requests must go out as `SET_REPORT` control transfers.
- **What they could have done:** Exposed standard USB HID Usage Page `0x85` on the receiver just as they already expose GATT `0x180F` over BLE.
- **Driver:** `src/vendors/keychron.rs` (from `keychron-battery-dkms`, `tailglow` and Keychron Launcher's web app). On a Link-KM (`3434:d029`) the paired-device lookup (`B5 03`) is verified and names the mouse (an M6, `3434:d060`), but that mouse answers neither `B3 06` nor `51 06`; its battery read is still unknown.
- **Transport quirk:** during testing a Link-KM answered only interrupt OUT in one power cycle and only `SET_REPORT` in another, so `scintilla` tries both. Keychron's Launcher fails on this M6 with *"The driver cannot be used after the device enters sleep mode"* for the same reason `scintilla` does: the mouse never answers `B3 06`. A `usbhid.quirks` boot parameter does **not** fix it.

### Logitech (VID `0x046D`)
- **What they do:** Proprietary HID++ 1.0/2.0 protocol with three separate battery feature IDs (`0x1000` Battery Status, `0x1001` Battery Voltage, `0x1004` Unified Battery) requiring runtime feature-index discovery, plus Logitech G headsets that abandon HID++ for separate raw HID reports.
- **Partial credit:** Linux's mainline `hid-logitech-hidpp` kernel driver covers a wide range of Unifying/Bolt/Lightspeed devices via `sysfs`, which `scintilla` prefers whenever available.
- **Driver:** `src/vendors/logitech.rs` (userspace HID++ 2.0 fallback).

### Sony — Audio Division (WH-1000XM / WF-1000XM)
- **What they do:** Refuse to report multi-cell (Left/Right/Case) battery telemetry over BLE GATT `0x180F`, requiring a custom framed binary protocol with additive checksums over Classic Bluetooth RFCOMM/SPP (`96cc203e-5068-46ad-b32d-e316f5e069ba`).
- **Partial credit:** Sony's PlayStation team upstreamed an official Linux kernel driver (`hid-playstation`) for DualSense/DualShock controllers, which works out of the box via `sysfs`.
- **Driver:** `src/vendors/sony_audio.rs` (based on `sony-headphones-control-linux`).

### Samsung — Galaxy Buds
- **What they do:** Ignore BLE GATT `0x180F` for multi-cell telemetry and emit Left/Right/Case battery inside framed binary `0x61` (`MSG_ID_EXTENDED_STATUS_UPDATED`) packets over Classic Bluetooth RFCOMM/SPP.
- **Driver:** `src/vendors/samsung_buds.rs` (based on `GalaxyBudsClient`).

### Bose (QuietComfort / Ultra)
- **What they do:** Wrap battery queries inside BMAP (Bose Mobile Audio Protocol, `[0x02, 0x02, 0x01, 0x00]`) over RFCOMM/BLE instead of exposing standard GATT `0x180F`.
- **Driver:** `src/vendors/bose.rs`.

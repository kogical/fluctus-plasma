# scintilla

**Async connected-device discovery and battery level reading for Rust.**

`scintilla` *(Latin: "spark")* is a standalone, fully async (Tokio) crate for discovering connected peripherals and reading their battery levels — including multi-cell devices (earbuds + charging case, split keyboards, dual-battery docks) and connected "stub" devices whose battery cannot be read so your UI can still show them as connected.

It lives inside the [Fluctus](https://github.com/kogical/fluctus) monorepo for convenience, with zero Plasma, Qt, or Fluctus dependencies so it can be used independently or extracted cleanly at any time.

---

## Standardized Interfaces Supported

`scintilla` always prefers open, standardized OS and hardware interfaces first:

- **Linux `sysfs` (`/sys/class/power_supply/*`)** — Laptops, AC/USB-C PD, and every peripheral backed by a mainline kernel driver (`hid-playstation`, `hid-logitech-hidpp`, `hid-apple`, `hid-nintendo`, `hid-steelseries`, `hid-corsair-void`, `hid-wacom`, `xpadneo`, `CONFIG_HID_BATTERY_STRENGTH`).
- **UPower D-Bus (`org.freedesktop.UPower`)** — Unprivileged, event-driven desktop power aggregator.
- **BlueZ D-Bus (`org.bluez.Device1` & `org.bluez.Battery1`)** — Discovers every connected Bluetooth device (including stubs) and surfaces GATT Battery Service + PipeWire/WirePlumber HFP (`AT+BIEV`, `AT+IPHONEACCEV`, `AT+XEVENT`) battery reports.
- **BLE GATT Battery Service (`0x180F` / `0x2A19`)** — Reads GATT battery characteristics directly via BlueZ D-Bus, unlocking multi-cell BLE peripherals (such as ZMK split keyboards) that `org.bluez.Battery1` flattens to a single percentage.
- **USB HID Power Device / Battery System (`Usage Page 0x84` / `0x85`)** — Pure-Rust Linux `hidraw` descriptor scanner and `RemainingCapacity` (`0x66`) reader for compliant UPSes and HID peripherals.
- **Linux `sysfs` HID Discovery** — Surfaces connected USB/HID peripherals as structured stubs (`PowerState::Unavailable`) even when no battery protocol is available.

For hardware from manufacturers that ignore these standards, `scintilla` also includes opt-in, modular vendor drivers under `src/vendors/` — see [HALL-OF-SHAME.md](HALL-OF-SHAME.md).

---

## Quick Start

```rust
use std::time::Duration;
use scintilla::{Manager, PowerState, WatchEvent, WatchMode};

#[tokio::main]
async fn main() -> scintilla::Result<()> {
    let manager = Manager::new().await?;

    // 1. Oneshot discovery (returns both readable batteries and connected stubs)
    for device in manager.devices().await? {
        match &device.power {
            PowerState::Available(report) => {
                println!("{}: {:?}% ({:?})", device.name, report.primary_percentage(), report.source);
            }
            PowerState::Unavailable { reason } => {
                println!("{}: connected, no battery read ({reason:?})", device.name);
            }
        }
    }

    // 2. Oneshot lookup by native ID (Bluetooth MAC, /dev/hidraw*, /sys/..., or usb:vid:pid).
    //    `scintilla` is primarily about reading batteries — if your app already uses another
    //    crate or stack for device discovery, running two separate discovery layers and
    //    mapping foreign types into `scintilla::Device` just to check a battery is pointless.
    //    Pass the native ID directly to use `scintilla`'s battery readers without being tied
    //    to its discovery pipeline.
    if let Some(power) = manager.power_by_id("AA:BB:CC:DD:EE:FF").await? {
        println!("Power state: {power:?}");
    }

    // 3. Reactive watch: Auto (event-driven if available, poll only if needed),
    //    WatchMode::EventOnly, or WatchMode::PollingOnly(interval).
    // Dropping `watch` immediately stops all event listeners and polling for this device.
    let mut watch = manager
        .watch_id(
            "AA:BB:CC:DD:EE:FF",
            WatchMode::Auto { poll_interval: Duration::from_secs(60) },
        )
        .await?;

    println!("Strategy: {:?} (polling={})", watch.strategy(), watch.is_polling());
    while let Some(event) = watch.recv().await {
        match event {
            WatchEvent::Updated { device, power } => println!("{} -> {power:?}", device.name),
            WatchEvent::Error(err) => eprintln!("Read error: {err}"),
            WatchEvent::Disconnected(addr) => println!("Disconnected: {addr:?}"),
        }
    }

    Ok(())
}
```

---

## Feature Flags

| Feature | Default | Description |
| :--- | :---: | :--- |
| `sysfs` | Yes | `/sys/class/power_supply/*` and `/sys/class/hidraw/*` discovery |
| `upower` | Yes | `org.freedesktop.UPower` D-Bus provider (`zbus`) |
| `bluez` | Yes | `org.bluez.Device1`, `Battery1`, direct BLE GATT `0x180F`, and D-Bus `PropertiesChanged` watches |
| `hidraw` | Yes | Pure-Rust Linux `/dev/hidraw*` ioctls + standard HID Usage Page `0x84`/`0x85` |
| `rfcomm` | via `vendors` | Pure-Rust Linux `AF_BLUETOOTH` RFCOMM client sockets |
| `vendors` | Yes | Enables all proprietary vendor quirk modules (`vendor-keychron`, `vendor-logitech`, `vendor-razer`, `vendor-corsair`, `vendor-steelseries`, `vendor-apple-airpods`, `vendor-sony-audio`, `vendor-samsung-buds`, `vendor-bose`) |
| `serde` | No | `Serialize`/`Deserialize` derives for public model types |

---

## Hardware Support & Contributions

Pull requests for additional devices, protocol fixes, and new vendor modules are very welcome!

> **Note on proprietary vendor modules:** Most of the vendor-specific drivers in `src/vendors/` have **not** been tested on physical hardware by the maintainers and are best-effort implementations written from community documentation and reverse-engineering references. If a device fails to report or returns incorrect values, please open an issue or PR with a capture or fix — see [docs/adding-vendors.md](docs/adding-vendors.md).

## Documentation

- [docs/architecture.md](docs/architecture.md) — Identity correlation, `BatteryWatch` RAII design, and permissions (`udev/70-scintilla.rules`).
- [docs/adding-vendors.md](docs/adding-vendors.md) — How to add a new proprietary vendor module cleanly.
- [HALL-OF-SHAME.md](HALL-OF-SHAME.md) — Manufacturers that ignore open battery standards and what their protocols do instead.

## License

Dual-licensed under either **Apache-2.0** ([LICENSE-APACHE](../../LICENSE)) or **MIT** ([LICENSE-MIT](LICENSE-MIT)), at your option.

// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! Pure-Rust async Linux `/dev/hidraw*` transport and standard USB HID Power
//! Device / Battery System (`Usage Page 0x84` / `0x85`) reader.

use std::fs::OpenOptions;
use std::io::{Read, Write};
use std::os::fd::AsRawFd;
use std::path::{Path, PathBuf};
use std::time::Duration;

use crate::error::{Error, Result};
use crate::model::{BatteryReport, ChargingState, ReadSource};

/// Standard USB HID Power Device Usage Page (`0x84`).
pub const HID_USAGE_PAGE_POWER_DEVICE: u16 = 0x0084;
/// Standard USB HID Battery System Usage Page (`0x85`).
pub const HID_USAGE_PAGE_BATTERY_SYSTEM: u16 = 0x0085;
/// Usage `0x66` on Battery System Page: `RemainingCapacity`.
const USAGE_REMAINING_CAPACITY: u16 = 0x0066;

/// Async handle to a Linux `/dev/hidraw*` device.
#[derive(Debug, Clone)]
pub struct HidrawDevice {
    path: PathBuf,
}

impl HidrawDevice {
    /// Creates a handle targeting `path` (e.g. `/dev/hidraw3`).
    #[must_use]
    pub fn new(path: impl Into<PathBuf>) -> Self {
        Self { path: path.into() }
    }

    /// Returns the `/dev/hidraw*` path.
    #[must_use]
    pub fn path(&self) -> &Path {
        &self.path
    }

    /// Reads the raw HID report descriptor via `HIDIOCGRDESC`.
    pub async fn report_descriptor(&self) -> Result<Vec<u8>> {
        let path = self.path.clone();
        tokio::task::spawn_blocking(move || read_report_descriptor_blocking(&path))
            .await
            .map_err(|e| Error::Io(std::io::Error::other(e)))?
    }

    /// Issues a `HIDIOCGFEATURE` ioctl. `buf[0]` must contain the Report ID.
    pub async fn get_feature_report(&self, mut buf: Vec<u8>) -> Result<Vec<u8>> {
        let path = self.path.clone();
        tokio::task::spawn_blocking(move || {
            let file = open_hidraw(&path, true)?;
            #[cfg(target_os = "linux")]
            {
                let req = hidiocgfeature(buf.len());
                // SAFETY: `file` is a valid hidraw fd and `buf` is a valid mutable byte buffer
                // whose length matches the ioctl size encoding.
                let rc = unsafe { libc::ioctl(file.as_raw_fd(), req, buf.as_mut_ptr()) };
                if rc < 0 {
                    return Err(map_io_err(&path, std::io::Error::last_os_error()));
                }
                buf.truncate(rc as usize);
                Ok(buf)
            }
            #[cfg(not(target_os = "linux"))]
            {
                let _ = file;
                Err(Error::Unsupported("hidraw is Linux-only"))
            }
        })
        .await
        .map_err(|e| Error::Io(std::io::Error::other(e)))?
    }

    /// Issues a `HIDIOCSFEATURE` ioctl. `buf[0]` must contain the Report ID.
    pub async fn set_feature_report(&self, buf: Vec<u8>) -> Result<usize> {
        let path = self.path.clone();
        tokio::task::spawn_blocking(move || {
            let file = open_hidraw(&path, true)?;
            #[cfg(target_os = "linux")]
            {
                let req = hidiocsfeature(buf.len());
                // SAFETY: `file` is a valid hidraw fd and `buf` is valid for `buf.len()` bytes.
                let rc = unsafe { libc::ioctl(file.as_raw_fd(), req, buf.as_ptr()) };
                if rc < 0 {
                    return Err(map_io_err(&path, std::io::Error::last_os_error()));
                }
                Ok(rc as usize)
            }
            #[cfg(not(target_os = "linux"))]
            {
                let _ = (file, buf);
                Err(Error::Unsupported("hidraw is Linux-only"))
            }
        })
        .await
        .map_err(|e| Error::Io(std::io::Error::other(e)))?
    }

    /// Writes an output report and waits up to `timeout` for a matching input report.
    pub async fn exchange_report(
        &self,
        request: Vec<u8>,
        response_len: usize,
        timeout: Duration,
    ) -> Result<Vec<u8>> {
        let path = self.path.clone();
        tokio::task::spawn_blocking(move || {
            let mut file = open_hidraw(&path, true)?;
            file.write_all(&request)
                .map_err(|e| map_io_err(&path, e))?;

            #[cfg(target_os = "linux")]
            {
                let mut pfd = libc::pollfd {
                    fd: file.as_raw_fd(),
                    events: libc::POLLIN,
                    revents: 0,
                };
                let timeout_ms = timeout.as_millis().min(i32::MAX as u128) as libc::c_int;
                // SAFETY: `pfd` points to a valid stack `pollfd`.
                let ready = unsafe { libc::poll(&mut pfd, 1, timeout_ms) };
                if ready == 0 {
                    return Err(Error::Timeout(path.display().to_string()));
                }
                if ready < 0 {
                    return Err(map_io_err(&path, std::io::Error::last_os_error()));
                }
            }

            let mut buf = vec![0u8; response_len];
            let n = file.read(&mut buf).map_err(|e| map_io_err(&path, e))?;
            buf.truncate(n);
            Ok(buf)
        })
        .await
        .map_err(|e| Error::Io(std::io::Error::other(e)))?
    }

    /// Attempts to read a standard USB HID Battery System (`Usage Page 0x85`)
    /// `RemainingCapacity` (`0x66`) feature report from this device.
    pub async fn read_standard_hid_battery(&self) -> Result<Option<BatteryReport>> {
        let desc = self.report_descriptor().await?;
        let Some(report_id) = find_remaining_capacity_report_id(&desc) else {
            return Ok(None);
        };
        let mut req = vec![0u8; 8];
        req[0] = report_id;
        let resp = self.get_feature_report(req).await?;
        let pct = resp.get(1).copied().ok_or_else(|| Error::Protocol {
            source_name: "hid-power-page",
            detail: "empty feature report response".into(),
        })?;
        if pct > 100 {
            return Ok(None);
        }
        Ok(Some(BatteryReport::single(
            f32::from(pct),
            ChargingState::Unknown,
            ReadSource::HidPowerPage,
        )))
    }
}

fn open_hidraw(path: &Path, write: bool) -> Result<std::fs::File> {
    OpenOptions::new()
        .read(true)
        .write(write)
        .open(path)
        .map_err(|e| map_io_err(path, e))
}

fn map_io_err(path: &Path, err: std::io::Error) -> Error {
    if err.kind() == std::io::ErrorKind::PermissionDenied {
        Error::PermissionDenied {
            path: path.to_path_buf(),
        }
    } else {
        Error::Io(err)
    }
}

#[cfg(target_os = "linux")]
const fn ioc(dir: u32, ty: u32, nr: u32, size: usize) -> libc::Ioctl {
    ((dir << 30) | (ty << 8) | nr | ((size as u32) << 16)) as libc::Ioctl
}

#[cfg(target_os = "linux")]
const fn hidiocsfeature(len: usize) -> libc::Ioctl {
    ioc(3, b'H' as u32, 0x06, len)
}

#[cfg(target_os = "linux")]
const fn hidiocgfeature(len: usize) -> libc::Ioctl {
    ioc(3, b'H' as u32, 0x07, len)
}

#[cfg(target_os = "linux")]
#[repr(C)]
struct HidrawReportDescriptor {
    size: u32,
    value: [u8; 4096],
}

fn read_report_descriptor_blocking(path: &Path) -> Result<Vec<u8>> {
    let file = open_hidraw(path, false)?;
    #[cfg(target_os = "linux")]
    {
        let mut size: libc::c_int = 0;
        let req_size = ioc(2, b'H' as u32, 0x01, std::mem::size_of::<libc::c_int>());
        // SAFETY: `file` is a valid hidraw fd and `size` is a valid `c_int`.
        if unsafe { libc::ioctl(file.as_raw_fd(), req_size, &mut size) } < 0 {
            return Err(map_io_err(path, std::io::Error::last_os_error()));
        }
        let clamped = (size.max(0) as usize).min(4096);
        let mut desc = HidrawReportDescriptor {
            size: clamped as u32,
            value: [0u8; 4096],
        };
        let req_desc = ioc(
            2,
            b'H' as u32,
            0x02,
            std::mem::size_of::<HidrawReportDescriptor>(),
        );
        // SAFETY: `desc` matches `struct hidraw_report_descriptor` in `<linux/hidraw.h>`.
        if unsafe { libc::ioctl(file.as_raw_fd(), req_desc, &mut desc) } < 0 {
            return Err(map_io_err(path, std::io::Error::last_os_error()));
        }
        Ok(desc.value[..clamped].to_vec())
    }
    #[cfg(not(target_os = "linux"))]
    {
        let _ = file;
        Err(Error::Unsupported("hidraw is Linux-only"))
    }
}

/// Scans a USB HID report descriptor for a Feature report exposing
/// `Usage Page (Battery System 0x85)` + `Usage (RemainingCapacity 0x66)`.
#[must_use]
pub fn find_remaining_capacity_report_id(desc: &[u8]) -> Option<u8> {
    let mut i = 0;
    let mut usage_page: u16 = 0;
    let mut current_report_id: u8 = 0;
    let mut saw_remaining_capacity = false;

    while i < desc.len() {
        let prefix = desc[i];
        if prefix == 0xFE {
            // Long item: [0xFE, data_size, long_item_tag, data...]
            let len = desc.get(i + 1).copied().unwrap_or(0) as usize;
            i = i.saturating_add(3 + len);
            continue;
        }
        let size_code = prefix & 0x03;
        let data_len = match size_code {
            3 => 4,
            n => n as usize,
        };
        let item_type = (prefix >> 2) & 0x03;
        let tag = (prefix >> 4) & 0x0F;

        if i + 1 + data_len > desc.len() {
            break;
        }
        let data = &desc[i + 1..i + 1 + data_len];
        let val = match data_len {
            1 => u32::from(data[0]),
            2 => u32::from(u16::from_le_bytes([data[0], data[1]])),
            4 => u32::from_le_bytes([data[0], data[1], data[2], data[3]]),
            _ => 0,
        };

        match (item_type, tag) {
            // Global: Usage Page (0x04)
            (1, 0x00) => usage_page = val as u16,
            // Global: Report ID (0x84)
            (1, 0x08) => current_report_id = val as u8,
            // Local: Usage (0x08)
            (2, 0x00) => {
                if usage_page == HID_USAGE_PAGE_BATTERY_SYSTEM
                    && (val as u16) == USAGE_REMAINING_CAPACITY
                {
                    saw_remaining_capacity = true;
                }
            }
            // Main: Feature (0xB0)
            (0, 0x0B) => {
                if saw_remaining_capacity {
                    return Some(current_report_id);
                }
            }
            // Main: Input / Output reset local usages
            (0, 0x08 | 0x09) => saw_remaining_capacity = false,
            _ => {}
        }

        i += 1 + data_len;
    }

    None
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn finds_battery_system_remaining_capacity_report_id() {
        // Usage Page (Battery System 0x85), Report ID (0x0C), Usage (RemainingCapacity 0x66), Feature (Data,Var,Abs)
        let desc = [0x05, 0x85, 0x85, 0x0C, 0x09, 0x66, 0xB1, 0x02];
        assert_eq!(find_remaining_capacity_report_id(&desc), Some(0x0C));
    }

    #[test]
    fn ignores_non_battery_descriptors() {
        // Usage Page (Generic Desktop 0x01), Usage (Mouse 0x02), Input
        let desc = [0x05, 0x01, 0x09, 0x02, 0x81, 0x02];
        assert_eq!(find_remaining_capacity_report_id(&desc), None);
    }
}

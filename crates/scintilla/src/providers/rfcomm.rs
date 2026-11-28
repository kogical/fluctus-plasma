// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! Pure-Rust async Linux `AF_BLUETOOTH` (`BTPROTO_RFCOMM`) client socket.
//!
//! Used by proprietary audio vendor modules (Sony Headphones, Samsung Galaxy Buds,
//! Bose BMAP) that refuse to expose battery telemetry over standard GATT BAS.

use std::time::Duration;

use crate::error::{Error, Result};

/// Sends `request` over a Bluetooth RFCOMM channel to `mac` and reads one response packet.
pub async fn rfcomm_exchange(
    mac: [u8; 6],
    channel: u8,
    request: Vec<u8>,
    max_response: usize,
    timeout: Duration,
) -> Result<Vec<u8>> {
    tokio::task::spawn_blocking(move || {
        rfcomm_exchange_blocking(mac, channel, &request, max_response, timeout)
    })
    .await
    .map_err(|e| Error::Io(std::io::Error::other(e)))?
}

#[cfg(target_os = "linux")]
#[repr(C)]
struct SockAddrRc {
    rc_family: libc::sa_family_t,
    rc_bdaddr: [u8; 6],
    rc_channel: u8,
}

#[cfg(target_os = "linux")]
fn rfcomm_exchange_blocking(
    mac: [u8; 6],
    channel: u8,
    request: &[u8],
    max_response: usize,
    timeout: Duration,
) -> Result<Vec<u8>> {
    const BTPROTO_RFCOMM: libc::c_int = 3;

    // SAFETY: Standard POSIX socket creation for Linux AF_BLUETOOTH / RFCOMM.
    let fd = unsafe { libc::socket(libc::AF_BLUETOOTH, libc::SOCK_STREAM, BTPROTO_RFCOMM) };
    if fd < 0 {
        return Err(Error::Io(std::io::Error::last_os_error()));
    }

    struct FdGuard(libc::c_int);
    impl Drop for FdGuard {
        fn drop(&mut self) {
            // SAFETY: `self.0` is an open file descriptor owned by this guard.
            unsafe {
                libc::close(self.0);
            }
        }
    }
    let _guard = FdGuard(fd);

    let tv = libc::timeval {
        tv_sec: timeout.as_secs() as libc::time_t,
        tv_usec: timeout.subsec_micros() as libc::suseconds_t,
    };
    // SAFETY: `tv` is a valid `timeval` for `SO_RCVTIMEO` and `SO_SNDTIMEO`.
    unsafe {
        libc::setsockopt(
            fd,
            libc::SOL_SOCKET,
            libc::SO_RCVTIMEO,
            (&raw const tv).cast(),
            std::mem::size_of::<libc::timeval>() as libc::socklen_t,
        );
        libc::setsockopt(
            fd,
            libc::SOL_SOCKET,
            libc::SO_SNDTIMEO,
            (&raw const tv).cast(),
            std::mem::size_of::<libc::timeval>() as libc::socklen_t,
        );
    }

    // BlueZ expects bdaddr bytes in reverse (little-endian) order.
    let mut bdaddr = mac;
    bdaddr.reverse();
    let addr = SockAddrRc {
        rc_family: libc::AF_BLUETOOTH as libc::sa_family_t,
        rc_bdaddr: bdaddr,
        rc_channel: channel,
    };

    // SAFETY: `addr` matches `struct sockaddr_rc` in `<bluetooth/rfcomm.h>`.
    let rc = unsafe {
        libc::connect(
            fd,
            (&raw const addr).cast::<libc::sockaddr>(),
            std::mem::size_of::<SockAddrRc>() as libc::socklen_t,
        )
    };
    if rc < 0 {
        return Err(Error::Io(std::io::Error::last_os_error()));
    }

    if !request.is_empty() {
        // SAFETY: `request` is a valid byte slice.
        let written = unsafe { libc::write(fd, request.as_ptr().cast(), request.len()) };
        if written < 0 {
            return Err(Error::Io(std::io::Error::last_os_error()));
        }
    }

    let mut buf = vec![0u8; max_response];
    // SAFETY: `buf` is a valid mutable byte buffer.
    let n = unsafe { libc::read(fd, buf.as_mut_ptr().cast(), buf.len()) };
    if n < 0 {
        return Err(Error::Io(std::io::Error::last_os_error()));
    }
    buf.truncate(n as usize);
    Ok(buf)
}

#[cfg(not(target_os = "linux"))]
fn rfcomm_exchange_blocking(
    _mac: [u8; 6],
    _channel: u8,
    _request: &[u8],
    _max_response: usize,
    _timeout: Duration,
) -> Result<Vec<u8>> {
    Err(Error::Unsupported("RFCOMM sockets are Linux-only"))
}

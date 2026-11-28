// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Framework-agnostic, zero-copy Apple-accurate Liquid Glass GPU rendering engine.

pub mod core;
pub mod shaders;

#[cfg(feature = "wgpu")]
pub mod wgpu_backend;

#[cfg(feature = "gl")]
pub mod gl_backend;

#[cfg(feature = "ffi")]
pub mod ffi;

pub use crate::core::*;

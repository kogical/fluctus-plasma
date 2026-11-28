// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Pre-translated shader sources emitted at build time by `naga` from the single WGSL source.
// Every backend (WebGPU/Vulkan/Metal/DX12/OpenGL/GLES) runs identical optical math with zero
// runtime shader translation overhead.

/// Validated WGSL sources (for `wgpu` / WebGPU).
pub mod wgsl {
    pub const KAWASE_DOWN: &str = include_str!(concat!(env!("OUT_DIR"), "/kawase_down.wgsl"));
    pub const KAWASE_UP: &str = include_str!(concat!(env!("OUT_DIR"), "/kawase_up.wgsl"));
    pub const LIQUIDGLASS: &str = include_str!(concat!(env!("OUT_DIR"), "/liquidglass.wgsl"));
}

/// Precompiled SPIR-V binaries (little-endian bytes, for Vulkan / OpenGL SPIR-V hosts).
pub mod spirv {
    pub const KAWASE_DOWN: &[u8] = include_bytes!(concat!(env!("OUT_DIR"), "/kawase_down.spv"));
    pub const KAWASE_UP: &[u8] = include_bytes!(concat!(env!("OUT_DIR"), "/kawase_up.spv"));
    pub const LIQUIDGLASS: &[u8] = include_bytes!(concat!(env!("OUT_DIR"), "/liquidglass.spv"));
}

/// Desktop OpenGL 3.30 core GLSL sources.
pub mod glsl330 {
    pub const KAWASE_DOWN_VERT: &str = include_str!(concat!(env!("OUT_DIR"), "/kawase_down.glsl330.vert"));
    pub const KAWASE_DOWN_FRAG: &str = include_str!(concat!(env!("OUT_DIR"), "/kawase_down.glsl330.frag"));
    pub const KAWASE_UP_VERT: &str = include_str!(concat!(env!("OUT_DIR"), "/kawase_up.glsl330.vert"));
    pub const KAWASE_UP_FRAG: &str = include_str!(concat!(env!("OUT_DIR"), "/kawase_up.glsl330.frag"));
    pub const LIQUIDGLASS_VERT: &str = include_str!(concat!(env!("OUT_DIR"), "/liquidglass.glsl330.vert"));
    pub const LIQUIDGLASS_FRAG: &str = include_str!(concat!(env!("OUT_DIR"), "/liquidglass.glsl330.frag"));
}

/// OpenGL ES 3.00 GLSL sources (embedded Wayland, Android, WebGL2).
pub mod gles300 {
    pub const KAWASE_DOWN_VERT: &str = include_str!(concat!(env!("OUT_DIR"), "/kawase_down.gles300.vert"));
    pub const KAWASE_DOWN_FRAG: &str = include_str!(concat!(env!("OUT_DIR"), "/kawase_down.gles300.frag"));
    pub const KAWASE_UP_VERT: &str = include_str!(concat!(env!("OUT_DIR"), "/kawase_up.gles300.vert"));
    pub const KAWASE_UP_FRAG: &str = include_str!(concat!(env!("OUT_DIR"), "/kawase_up.gles300.frag"));
    pub const LIQUIDGLASS_VERT: &str = include_str!(concat!(env!("OUT_DIR"), "/liquidglass.gles300.vert"));
    pub const LIQUIDGLASS_FRAG: &str = include_str!(concat!(env!("OUT_DIR"), "/liquidglass.gles300.frag"));
}

/// Apple Metal Shading Language (MSL 2.1) sources.
pub mod msl {
    pub const KAWASE_DOWN: &str = include_str!(concat!(env!("OUT_DIR"), "/kawase_down.metal"));
    pub const KAWASE_UP: &str = include_str!(concat!(env!("OUT_DIR"), "/kawase_up.metal"));
    pub const LIQUIDGLASS: &str = include_str!(concat!(env!("OUT_DIR"), "/liquidglass.metal"));
}

/// DirectX 12 HLSL (Shader Model 5.1) sources.
pub mod hlsl {
    pub const KAWASE_DOWN: &str = include_str!(concat!(env!("OUT_DIR"), "/kawase_down.hlsl"));
    pub const KAWASE_UP: &str = include_str!(concat!(env!("OUT_DIR"), "/kawase_up.hlsl"));
    pub const LIQUIDGLASS: &str = include_str!(concat!(env!("OUT_DIR"), "/liquidglass.hlsl"));
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn all_six_naga_targets_are_non_empty() {
        assert!(wgsl::LIQUIDGLASS.contains("fn fs_main"));
        assert!(spirv::LIQUIDGLASS.len() > 256);
        assert!(glsl330::LIQUIDGLASS_VERT.contains("#version 330 core"));
        assert!(glsl330::LIQUIDGLASS_FRAG.contains("#version 330 core"));
        assert!(gles300::LIQUIDGLASS_FRAG.contains("#version 300 es"));
        assert!(msl::LIQUIDGLASS.contains("metal_stdlib"));
        assert!(hlsl::LIQUIDGLASS.contains("fs_main"));
    }
}

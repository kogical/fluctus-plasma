// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Dual Kawase blur downsample pass (single source compiled via naga to WGSL/SPIR-V/GLSL/MSL/HLSL).

struct KawaseUniforms {
    halfpixel: vec2<f32>,
    offset: f32,
    flip_y: f32, // > 0.5: flip input texture V coordinate
};

@group(0) @binding(0) var<uniform> u: KawaseUniforms;
@group(0) @binding(1) var src_tex: texture_2d<f32>;
@group(0) @binding(2) var src_samp: sampler;

struct VsOut {
    @builtin(position) pos: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) vid: u32) -> VsOut {
    // Fullscreen triangle covering clip space [-1, 1] x [-1, 1].
    var p = vec2<f32>(-1.0, -1.0);
    if (vid == 1u) {
        p = vec2<f32>(3.0, -1.0);
    } else if (vid == 2u) {
        p = vec2<f32>(-1.0, 3.0);
    }
    var out: VsOut;
    out.pos = vec4<f32>(p, 0.0, 1.0);
    var uv = p * 0.5 + vec2<f32>(0.5, 0.5);
    if (u.flip_y > 0.5) {
        uv.y = 1.0 - uv.y;
    }
    out.uv = uv;
    return out;
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4<f32> {
    let h = u.halfpixel * u.offset;
    let uv = in.uv;
    var sum = textureSample(src_tex, src_samp, uv) * 4.0;
    sum += textureSample(src_tex, src_samp, uv - h);
    sum += textureSample(src_tex, src_samp, uv + h);
    sum += textureSample(src_tex, src_samp, uv + vec2<f32>(h.x, -h.y));
    sum += textureSample(src_tex, src_samp, uv - vec2<f32>(h.x, -h.y));
    return sum / 8.0;
}

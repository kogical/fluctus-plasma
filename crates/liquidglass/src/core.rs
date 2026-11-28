// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Zero-dependency core data structures, optics math, Kawase schedule, bounds, pointer physics,
// and GPU uniform packing for Liquid Glass. Every cross-host struct is `#[repr(C)]` so the exact
// same layout works in Rust, C/C++ (KWin, Qt, GTK), Dart FFI (Flutter), Swift, and WebGPU.

use bytemuck::{Pod, Zeroable};

/// Vertical texture coordinate origin of the host graphics API.
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum LgYOrigin {
    /// Vulkan, Metal, DX12, WebGPU, Wayland SHM, CPU pixel buffers (`v = 0` at top).
    #[default]
    TopLeft = 0,
    /// OpenGL / OpenGL ES textures & FBOs (`v = 0` at bottom).
    BottomLeft = 1,
}

/// Pixel format of host backdrop / target textures (supports SDR `RGBA8` and KWin HDR `RGBA16F`).
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum LgPixelFormat {
    #[default]
    Rgba8Unorm = 0,
    Bgra8Unorm = 1,
    Rgba16Float = 2,
    Rgba8UnormSrgb = 3,
}

/// Result code returned by C FFI entry points.
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LgStatus {
    Ok = 0,
    NullPointer = 1,
    InvalidArgument = 2,
    GlCompileError = 3,
    GlFramebufferError = 4,
    Unsupported = 5,
}

/// Straight-alpha RGBA color in the output color space (SDR `0..1`, or HDR linear/mapped).
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Default, Pod, Zeroable)]
pub struct LgColor {
    pub r: f32,
    pub g: f32,
    pub b: f32,
    pub a: f32,
}

impl LgColor {
    pub const TRANSPARENT: Self = Self { r: 0.0, g: 0.0, b: 0.0, a: 0.0 };

    #[inline]
    pub const fn new(r: f32, g: f32, b: f32, a: f32) -> Self {
        Self { r, g, b, a }
    }

    #[inline]
    pub const fn to_array(self) -> [f32; 4] {
        [self.r, self.g, self.b, self.a]
    }
}

/// Floating-point rectangle in capture-local device pixels (y pointing down).
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Default, Pod, Zeroable)]
pub struct LgRect {
    pub x: f32,
    pub y: f32,
    pub width: f32,
    pub height: f32,
}

impl LgRect {
    #[inline]
    pub const fn new(x: f32, y: f32, width: f32, height: f32) -> Self {
        Self { x, y, width, height }
    }

    #[inline]
    pub const fn right(self) -> f32 {
        self.x + self.width
    }

    #[inline]
    pub const fn bottom(self) -> f32 {
        self.y + self.height
    }

    #[inline]
    pub fn is_empty(self) -> bool {
        self.width <= 0.0 || self.height <= 0.0
    }

    #[inline]
    pub fn expanded(self, margin: f32) -> Self {
        Self {
            x: self.x - margin,
            y: self.y - margin,
            width: self.width + 2.0 * margin,
            height: self.height + 2.0 * margin,
        }
    }

    #[inline]
    pub fn intersected(self, other: Self) -> Self {
        let x0 = self.x.max(other.x);
        let y0 = self.y.max(other.y);
        let x1 = self.right().min(other.right());
        let y1 = self.bottom().min(other.bottom());
        Self {
            x: x0,
            y: y0,
            width: (x1 - x0).max(0.0),
            height: (y1 - y0).max(0.0),
        }
    }
}

/// Integer rectangle (e.g. GL viewport / scissor box).
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default, Pod, Zeroable)]
pub struct LgRectI {
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
}

/// Physical Liquid Glass material parameters (all spatial lengths in logical pixels unless noted).
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct LgMaterial {
    /// Master effect strength `0..2` scaling refraction, magnification, dispersion, bulge, and rim.
    pub intensity: f32,
    /// Straight-alpha tint color blended over the graded refracted backdrop.
    pub tint: LgColor,
    /// Curved bezel width in logical px.
    pub bezel: f32,
    /// Max bezel refraction displacement in logical px.
    pub refraction: f32,
    /// Smooth pillow-dome magnification `0..0.45`.
    pub magnify: f32,
    /// Extra outer-lip refraction fold `0..1`.
    pub edge_bend: f32,
    /// Spectral Cauchy chromatic dispersion factor `0..1`.
    pub dispersion: f32,
    /// Interior backdrop diffusion strength `0..1` (0 = sharp, ~0.24 = Liquid Glass, 0.75+ = frosted).
    pub frost: f32,
    /// Backdrop saturation multiplier.
    pub saturation: f32,
    /// Backdrop brightness multiplier.
    pub brightness: f32,
    /// Optional mid-bezel refractive darkening band `0..1`.
    pub edge_shade: f32,
    /// Film grain `0..1`.
    pub grain: f32,
    /// Dual-corner specular rim intensity (`CASDFGlassHighlightEffect`).
    pub highlight: f32,
    /// Specular rim Gaussian half-width in logical px.
    pub highlight_width: f32,
    /// Optional directional surface sheen `0..1`.
    pub sheen: f32,
    /// Key light angle in degrees (y down; `-135.0` = top-left key + bottom-right opposite bounce).
    pub light_angle_deg: f32,
    /// Optional inner hairline border (straight-alpha RGBA).
    pub inner_border: LgColor,
    /// Optional outer hairline border (straight-alpha RGBA).
    pub outer_border: LgColor,
    /// Border hairline width in logical px.
    pub border_width: f32,
    /// Outside-only soft elevation drop shadow color (straight-alpha RGBA).
    pub shadow_color: LgColor,
    /// Drop shadow softness in logical px.
    pub shadow_blur: f32,
    /// Drop shadow X offset in logical px.
    pub shadow_offset_x: f32,
    /// Drop shadow Y offset in logical px.
    pub shadow_offset_y: f32,
    /// Pointer-driven rim swing + light spot strength `0..1`.
    pub pointer_light: f32,
    /// Pointer-driven water-drop bulge displacement in logical px.
    pub pointer_bulge: f32,
    /// Pointer bulge Gaussian radius in logical px.
    pub pointer_radius: f32,
}

impl Default for LgMaterial {
    /// Apple macOS 26 Tahoe / iOS 26 Liquid Glass defaults (verified by parity suite).
    fn default() -> Self {
        Self {
            intensity: 1.0,
            tint: LgColor::new(1.0, 1.0, 1.0, 0.015),
            bezel: 64.0,
            refraction: 60.0,
            magnify: 0.34,
            edge_bend: 0.15,
            dispersion: 0.74,
            frost: 0.24,
            saturation: 1.24,
            brightness: 1.02,
            edge_shade: 0.0,
            grain: 0.0,
            highlight: 0.84,
            highlight_width: 1.85,
            sheen: 0.0,
            light_angle_deg: -135.0,
            inner_border: LgColor::TRANSPARENT,
            outer_border: LgColor::TRANSPARENT,
            border_width: 1.0,
            shadow_color: LgColor::new(0.0, 0.0, 0.0, 0.09),
            shadow_blur: 36.0,
            shadow_offset_x: 0.0,
            shadow_offset_y: 8.0,
            pointer_light: 1.0,
            pointer_bulge: 6.0,
            pointer_radius: 56.0,
        }
    }
}

impl LgMaterial {
    /// Crystal preset: ultra-clear, sharp-edged thick crystal slab with zero interior blur.
    pub fn crystal() -> Self {
        Self {
            tint: LgColor::new(1.0, 1.0, 1.0, 0.01),
            bezel: 56.0,
            refraction: 54.0,
            magnify: 0.26,
            edge_bend: 0.25,
            dispersion: 0.65,
            frost: 0.0,
            saturation: 1.32,
            highlight: 1.05,
            highlight_width: 2.2,
            ..Self::default()
        }
    }

    /// Returns a copy with `intensity` folded into the optical strength fields (`intensity` becomes `1.0`).
    #[must_use]
    pub fn scaled(&self) -> Self {
        let mut m = *self;
        let k = m.intensity.clamp(0.0, 2.0);
        m.refraction *= k;
        m.magnify = (m.magnify * k).min(0.45);
        m.dispersion = (m.dispersion * k).min(1.0);
        m.pointer_bulge *= k;
        m.highlight *= k.sqrt();
        m.intensity = 1.0;
        m
    }

    /// Logical px around a shape whose backdrop the shader may sample (refraction + outer lip + dispersion + bulge).
    #[must_use]
    pub fn sample_margin(&self) -> f32 {
        (self.refraction * (1.75 + 0.70 * self.edge_bend) * (1.0 + self.dispersion)
            + self.pointer_bulge * 1.5
            + 8.0)
            .ceil()
    }

    /// Logical px around a shape the shader may draw into (shadow + outer border).
    #[must_use]
    pub fn draw_margin(&self) -> f32 {
        let mut margin = self.border_width + 2.0;
        if self.shadow_color.a > 0.001 {
            margin = margin.max(
                self.shadow_blur + self.shadow_offset_x.abs().max(self.shadow_offset_y.abs()),
            );
        }
        margin.ceil()
    }

    #[must_use]
    pub fn reacts_to_pointer(&self) -> bool {
        self.pointer_light > 0.001 || self.pointer_bulge > 0.001
    }
}

/// Interactive pointer state for a glass shape (in capture-local or shape-local device px).
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Default)]
pub struct LgPointerState {
    /// Pointer X relative to the shape's top-left corner (in device px).
    pub x: f32,
    /// Pointer Y relative to the shape's top-left corner (in device px).
    pub y: f32,
    /// Smoothly animated presence `0..1`.
    pub strength: f32,
}

impl LgPointerState {
    /// Advances `strength` smoothly toward `target` over `dt_seconds` (zero allocations, 0% idle CPU).
    /// Returns `true` if `strength` changed and another frame is needed.
    pub fn step_fade(&mut self, target: f32, dt_seconds: f32, fade_duration_seconds: f32) -> bool {
        let t = target.clamp(0.0, 1.0);
        if (self.strength - t).abs() <= 1e-5 {
            self.strength = t;
            return false;
        }
        let step = (dt_seconds.clamp(0.0, 0.1) / fade_duration_seconds.max(1e-4)).max(0.0);
        if self.strength < t {
            self.strength = (self.strength + step).min(t);
        } else {
            self.strength = (self.strength - step).max(t);
        }
        true
    }
}

/// One rounded-rect glass shape to render in capture-local device pixels.
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct LgShape {
    /// Glass rect `(x, y, width, height)` in capture-local device pixels (y down).
    pub rect: LgRect,
    /// Corner radius in device pixels.
    pub radius: f32,
    /// Output opacity `0..1`.
    pub opacity: f32,
    /// Device pixel scale factor (`scale = device_px / logical_px`, e.g. `1.0`, `1.5`, `2.0`).
    pub scale: f32,
    /// Material parameters (logical px; scaled by `scale` internally when packing uniforms).
    pub material: LgMaterial,
    /// Interactive pointer state (shape-local device px).
    pub pointer: LgPointerState,
}

/// Borrowed host backdrop texture descriptor (zero-copy).
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct LgBackdropDesc {
    /// Borrowed GL texture name (`GLuint`) when using the `gl` backend; ignored by `wgpu`.
    pub texture_id: u32,
    /// Full texture width in device pixels.
    pub width: u32,
    /// Full texture height in device pixels.
    pub height: u32,
    /// Normalized sub-rect `(u0, v0, u1, v1)` within the backdrop texture (default `0, 0, 1, 1`).
    pub uv_rect: [f32; 4],
    /// Vertical origin of the backdrop texture (`BottomLeft` for GL, `TopLeft` for Vulkan/Metal/CPU).
    pub y_origin: LgYOrigin,
    /// Pixel format of the backdrop texture (used to allocate matching internal scratch blur targets).
    pub format: LgPixelFormat,
}

impl Default for LgBackdropDesc {
    fn default() -> Self {
        Self {
            texture_id: 0,
            width: 0,
            height: 0,
            uv_rect: [0.0, 0.0, 1.0, 1.0],
            y_origin: LgYOrigin::BottomLeft,
            format: LgPixelFormat::Rgba8Unorm,
        }
    }
}

/// Borrowed host render target descriptor (zero-copy).
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct LgTargetDesc {
    /// Borrowed GL framebuffer handle (`GLuint`; `0` = default framebuffer).
    pub fbo_id: u32,
    /// Target viewport rectangle `(x, y, width, height)` in GL viewport coordinates.
    /// If `width == 0 || height == 0`, the currently bound GL viewport is preserved.
    pub viewport: LgRectI,
    /// Column-major 4x4 clip-space projection matrix mapping capture-local device px to NDC.
    /// If `use_custom_projection == 0`, an orthographic matrix is built from `backdrop` size & `y_origin`.
    pub projection: [f32; 16],
    /// Non-zero to use `projection`; `0` to auto-generate orthographic projection.
    pub use_custom_projection: u8,
    /// Non-zero if output should be premultiplied alpha (`GL_ONE, GL_ONE_MINUS_SRC_ALPHA`).
    pub premultiplied_output: u8,
    /// Vertical origin of the render target.
    pub y_origin: LgYOrigin,
    /// Pixel format of the render target.
    pub format: LgPixelFormat,
    /// Reference white in target color space (`[1.0, 1.0, 1.0]` for SDR, mapped white for KWin HDR).
    pub reference_white: [f32; 3],
    /// Optional damage clip rectangles in capture-local device px (null / 0 = full draw extent).
    pub clip_rects: *const LgRect,
    pub clip_rect_count: u32,
}

impl Default for LgTargetDesc {
    fn default() -> Self {
        Self {
            fbo_id: 0,
            viewport: LgRectI::default(),
            projection: [0.0; 16],
            use_custom_projection: 0,
            premultiplied_output: 1,
            y_origin: LgYOrigin::BottomLeft,
            format: LgPixelFormat::Rgba8Unorm,
            reference_white: [1.0, 1.0, 1.0],
            clip_rects: core::ptr::null(),
            clip_rect_count: 0,
        }
    }
}

/// Top-level render batch descriptor passed across the FFI boundary (zero allocations).
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct LgRenderParams {
    pub backdrop: LgBackdropDesc,
    pub target: LgTargetDesc,
    pub shapes: *const LgShape,
    pub shape_count: u32,
}

// -----------------------------------------------------------------------------
// Dual Kawase Blur Schedule (exact match to KWin Blur / Fluctus Glass)
// -----------------------------------------------------------------------------

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct KawaseStep {
    pub iterations: usize,
    pub offset: f32,
}

#[derive(Debug, Clone, Copy)]
struct KawaseLevel {
    min_offset: f32,
    max_offset: f32,
    expand_size: u32,
}

const KAWASE_LEVELS: [KawaseLevel; 4] = [
    KawaseLevel { min_offset: 1.0, max_offset: 2.0, expand_size: 10 },
    KawaseLevel { min_offset: 2.0, max_offset: 3.0, expand_size: 20 },
    KawaseLevel { min_offset: 2.0, max_offset: 5.0, expand_size: 50 },
    KawaseLevel { min_offset: 3.0, max_offset: 8.0, expand_size: 150 },
];

/// Maps `frost` (`0..1`) to KWin blur strength (`0` = sharp, `1..=15`).
#[must_use]
pub fn strength_for_frost(frost: f32) -> u32 {
    if frost < 0.01 {
        return 0;
    }
    let s = (15.0 * frost.clamp(0.0, 1.0).powf(1.5)).round() as i32;
    s.clamp(1, 15) as u32
}

/// Resolves a KWin blur strength (`0..=15`) + HiDPI scale into Dual Kawase `(iterations, offset)`.
#[must_use]
pub fn kawase_for_strength(strength: u32, scale: f32) -> KawaseStep {
    if strength == 0 {
        return KawaseStep { iterations: 0, offset: 0.0 };
    }
    const STEPS: i32 = 15;
    let sum: f32 = KAWASE_LEVELS.iter().map(|l| l.max_offset - l.min_offset).sum();
    let mut table = [KawaseStep { iterations: 1, offset: 1.0 }; 15];
    let mut idx = 0usize;
    let mut remaining = STEPS;
    for (i, level) in KAWASE_LEVELS.iter().enumerate() {
        let span = level.max_offset - level.min_offset;
        let mut count = (span / sum * STEPS as f32).ceil() as i32;
        remaining -= count;
        if remaining < 0 {
            count += remaining;
        }
        for j in 1..=count {
            if idx < table.len() {
                table[idx] = KawaseStep {
                    iterations: i + 1,
                    offset: level.min_offset + span / count as f32 * j as f32,
                };
                idx += 1;
            }
        }
    }
    let mut step = table[(strength.clamp(1, 15) - 1) as usize];
    if scale >= 1.5 && step.iterations < KAWASE_LEVELS.len() {
        step.iterations += 1;
    }
    step
}

/// Minimum backdrop capture expansion (in device px) required for `iterations` Dual Kawase passes.
#[must_use]
pub fn expand_for_iterations(iterations: usize) -> u32 {
    if iterations == 0 {
        0
    } else {
        KAWASE_LEVELS[iterations.min(KAWASE_LEVELS.len()) - 1].expand_size
    }
}

// -----------------------------------------------------------------------------
// Pure CPU Optics Math Mirror (unit-tested for exact parity with WGSL)
// -----------------------------------------------------------------------------

/// Continuous superellipse rounded-rect SDF (exact CPU mirror of `liquidglass.wgsl::sd_round_rect`).
#[must_use]
pub fn sd_round_rect(p: [f32; 2], half_size: [f32; 2], r: f32) -> f32 {
    let qx = p[0].abs() - half_size[0] + r;
    let qy = p[1].abs() - half_size[1] + r;
    if r > 0.5 && qx > 0.0 && qy > 0.0 {
        let min_h = half_size[0].min(half_size[1]).max(1.0);
        let t = ((r / min_h - 0.76) / (0.98 - 0.76)).clamp(0.0, 1.0);
        let circle_mix = t * t * (3.0 - 2.0 * t);
        let n = 2.2 + (2.0 - 2.2) * circle_mix;
        let ux = qx / r;
        let uy = qy / r;
        let s = (ux.powf(n) + uy.powf(n)).powf(1.0 / n);
        return (s - 1.0) * r;
    }
    qx.max(0.0).hypot(qy.max(0.0)) + qx.max(qy).min(0.0) - r
}

/// Builds a column-major orthographic projection mapping `(0..w, 0..h)` (y down) to NDC.
#[must_use]
pub fn ortho_projection(width: f32, height: f32, y_origin: LgYOrigin) -> [f32; 16] {
    let w = width.max(1.0);
    let h = height.max(1.0);
    // In both GL and Vulkan/Metal/WGPU clip space, NDC y=+1 is top (for TopLeft pixel y=0 -> NDC +1)
    // except GL framebuffer y=0 is bottom so TopLeft pixel y=0 still maps to NDC +1!
    // (When rendering to an offscreen FBO with BottomLeft origin, y=0 is bottom so pixel y=0 -> NDC +1
    //  places row 0 at the top of the screen when blitted!)
    let _ = y_origin;
    [
        2.0 / w, 0.0,      0.0, 0.0,
        0.0,    -2.0 / h,  0.0, 0.0,
        0.0,     0.0,      1.0, 0.0,
        -1.0,    1.0,      0.0, 1.0,
    ]
}

// -----------------------------------------------------------------------------
// GPU Uniform Blocks (16-byte std140 aligned, shared identically by wgpu & GL)
// -----------------------------------------------------------------------------

#[repr(C)]
#[derive(Debug, Clone, Copy, Pod, Zeroable)]
pub struct KawaseUniforms {
    pub halfpixel: [f32; 2],
    pub offset: f32,
    pub flip_y: f32,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, Pod, Zeroable)]
pub struct GlassUniforms {
    pub projection: [f32; 16],
    pub glass_rect: [f32; 4],
    pub uv_rect: [f32; 4],
    pub uv_clamp: [f32; 4],
    pub tint: [f32; 4],
    pub inner_border: [f32; 4],
    pub outer_border: [f32; 4],
    pub shadow_color: [f32; 4],
    pub tex_and_blur: [f32; 4],
    pub optics_a: [f32; 4],
    pub optics_b: [f32; 4],
    pub rim_params: [f32; 4],
    pub surface_a: [f32; 4],
    pub shadow_a: [f32; 4],
    pub pointer_a: [f32; 4],
    pub pointer_b: [f32; 4],
    pub misc: [f32; 4],
    pub white_ref: [f32; 4],
}

impl GlassUniforms {
    /// Packs a shape + backdrop + target + blur-level metadata into the exact GPU uniform block.
    #[must_use]
    pub fn pack(
        shape: &LgShape,
        backdrop: &LgBackdropDesc,
        target: &LgTargetDesc,
        use_blur: bool,
        blur_tex_size: [u32; 2],
        blur_offset: f32,
    ) -> Self {
        let m = shape.material.scaled();
        let s = shape.scale.max(0.01);
        let tw = backdrop.width.max(1) as f32;
        let th = backdrop.height.max(1) as f32;

        let projection = if target.use_custom_projection != 0 {
            target.projection
        } else {
            ortho_projection(tw, th, target.y_origin)
        };

        let uv_rect = if backdrop.uv_rect == [0.0, 0.0, 0.0, 0.0] {
            [0.0, 0.0, 1.0, 1.0]
        } else {
            backdrop.uv_rect
        };
        let hx = 0.5 / tw;
        let hy = 0.5 / th;
        let uv_clamp = [
            uv_rect[0].min(uv_rect[2]) + hx,
            uv_rect[1].min(uv_rect[3]) + hy,
            uv_rect[0].max(uv_rect[2]) - hx,
            uv_rect[1].max(uv_rect[3]) - hy,
        ];

        let angle = m.light_angle_deg.to_radians();
        let max_bezel = (shape.rect.width.min(shape.rect.height) / s) * 0.45;
        let clamped_bezel = m.bezel.min(max_bezel) * s;

        let bhx = 0.5 / blur_tex_size[0].max(1) as f32;
        let bhy = 0.5 / blur_tex_size[1].max(1) as f32;

        let ref_white = if target.reference_white == [0.0, 0.0, 0.0] {
            [1.0, 1.0, 1.0]
        } else {
            target.reference_white
        };

        Self {
            projection,
            glass_rect: [shape.rect.x, shape.rect.y, shape.rect.width, shape.rect.height],
            uv_rect,
            uv_clamp,
            tint: m.tint.to_array(),
            inner_border: m.inner_border.to_array(),
            outer_border: m.outer_border.to_array(),
            shadow_color: m.shadow_color.to_array(),
            tex_and_blur: [tw, th, bhx, bhy],
            optics_a: [
                shape.radius,
                clamped_bezel,
                m.refraction * s,
                m.dispersion,
            ],
            optics_b: [m.magnify, m.edge_bend, m.saturation, m.brightness],
            rim_params: [
                m.highlight,
                m.highlight_width * s,
                angle.cos(),
                angle.sin(),
            ],
            surface_a: [m.edge_shade, m.sheen, m.border_width * s, m.grain],
            shadow_a: [
                m.shadow_blur * s,
                m.shadow_offset_x * s,
                m.shadow_offset_y * s,
                1.0, // device pixel unit = 1.0
            ],
            pointer_a: [
                shape.pointer.x,
                shape.pointer.y,
                shape.pointer.strength.clamp(0.0, 1.0),
                m.pointer_light,
            ],
            pointer_b: [
                m.pointer_bulge * s,
                m.pointer_radius * s,
                shape.opacity.clamp(0.0, 1.0),
                if use_blur { 1.0 } else { 0.0 },
            ],
            misc: [
                blur_offset,
                if backdrop.y_origin == LgYOrigin::BottomLeft { 1.0 } else { 0.0 },
                if target.premultiplied_output != 0 { 1.0 } else { 0.0 },
                0.0,
            ],
            white_ref: [ref_white[0], ref_white[1], ref_white[2], 0.0],
        }
    }
}

/// Appends 2 triangles (6 `vec2<f32>` positions) for `rect` into `out` without allocating if capacity suffices.
pub fn push_quad_verts(out: &mut Vec<[f32; 2]>, rect: LgRect) {
    if rect.is_empty() {
        return;
    }
    let x0 = rect.x;
    let y0 = rect.y;
    let x1 = rect.right();
    let y1 = rect.bottom();
    out.push([x0, y0]);
    out.push([x1, y1]);
    out.push([x0, y1]);
    out.push([x0, y0]);
    out.push([x1, y0]);
    out.push([x1, y1]);
}

/// Appends `rect` clipped against `clips` (or unclipped if `clips` is empty).
pub fn push_clipped_quad_verts(out: &mut Vec<[f32; 2]>, rect: LgRect, clips: &[LgRect]) {
    if clips.is_empty() {
        push_quad_verts(out, rect);
        return;
    }
    for &clip in clips {
        let piece = rect.intersected(clip);
        if !piece.is_empty() {
            push_quad_verts(out, piece);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn uniform_layout_is_16_byte_aligned() {
        assert_eq!(core::mem::size_of::<KawaseUniforms>(), 16);
        assert_eq!(core::mem::size_of::<GlassUniforms>() % 16, 0);
        // 1 mat4 (64B) + 17 vec4 (272B) = 336 bytes
        assert_eq!(core::mem::size_of::<GlassUniforms>(), 336);
    }

    #[test]
    fn kawase_schedule_matches_kwin_table() {
        assert_eq!(strength_for_frost(0.0), 0);
        assert_eq!(strength_for_frost(0.24), 2);
        assert_eq!(strength_for_frost(1.0), 15);
        let step1 = kawase_for_strength(2, 1.0);
        assert_eq!(step1.iterations, 1);
        assert!(step1.offset >= 1.0 && step1.offset <= 2.0);
        let step_hidpi = kawase_for_strength(2, 2.0);
        assert_eq!(step_hidpi.iterations, 2);
        assert_eq!(expand_for_iterations(0), 0);
        assert_eq!(expand_for_iterations(1), 10);
        assert_eq!(expand_for_iterations(4), 150);
    }

    #[test]
    fn sdf_circle_is_exact_and_rect_is_continuous() {
        // Full circle (r == minHalf) must relax to n=2.0 -> exact Euclidean distance
        let d_45 = sd_round_rect([48.0 * 0.70710678, 48.0 * 0.70710678], [48.0, 48.0], 48.0);
        assert!(d_45.abs() < 0.05, "circle SDF at 45deg should be ~0, got {d_45}");
        // Rounded rect (r < minHalf) uses n=2.2 -> slightly fuller than circular arc at 45deg
        let d_rect_corner = sd_round_rect([200.0 - 50.0 + 50.0 * 0.70710678, 150.0 - 50.0 + 50.0 * 0.70710678], [200.0, 150.0], 50.0);
        assert!(d_rect_corner < -1.0, "continuous n=2.2 corner should bulge slightly outward, got {d_rect_corner}");
    }

    #[test]
    fn pointer_fade_settles_to_zero_idle_work() {
        let mut p = LgPointerState::default();
        assert!(p.step_fade(1.0, 0.07, 0.14));
        assert!((p.strength - 0.5).abs() < 1e-4);
        assert!(p.step_fade(1.0, 0.10, 0.14));
        assert_eq!(p.strength, 1.0);
        assert!(!p.step_fade(1.0, 0.016, 0.14));
    }
}

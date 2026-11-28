// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Zero-copy raw OpenGL / OpenGL ES 3.0 backend (`glow`).
// Borrows the host's active GL context, takes a borrowed `GLuint` backdrop texture and caller FBO,
// reuses pooled internal scratch Dual-Kawase blur textures (zero per-frame allocations), and
// strictly saves & restores EVERY touched GL state via RAII so host compositors (KWin, Qt, GTK,
// Flutter) never observe leaked GL state.

use std::num::NonZeroU32;

use glow::HasContext;

use crate::core::{
    kawase_for_strength, push_clipped_quad_verts, strength_for_frost, GlassUniforms,
    KawaseUniforms, LgPixelFormat, LgRect, LgRenderParams, LgShape, LgStatus, LgYOrigin,
};
use crate::shaders::{gles300, glsl330};

struct GlProgram {
    program: glow::Program,
    ubo_binding_ok: bool,
    loc_tex0: Option<glow::UniformLocation>,
    loc_tex1: Option<glow::UniformLocation>,
}

struct GlBlurLevel {
    texture: glow::Texture,
    fbo: glow::Framebuffer,
    width: u32,
    height: u32,
}

struct GlBlurPool {
    base_size: [u32; 2],
    format: LgPixelFormat,
    levels: Vec<GlBlurLevel>,
}

impl GlBlurPool {
    fn new() -> Self {
        Self { base_size: [0, 0], format: LgPixelFormat::Rgba8Unorm, levels: Vec::new() }
    }

    unsafe fn clear(&mut self, gl: &glow::Context) {
        for lvl in self.levels.drain(..) {
            unsafe {
                gl.delete_framebuffer(lvl.fbo);
                gl.delete_texture(lvl.texture);
            }
        }
        self.base_size = [0, 0];
    }

    unsafe fn ensure(
        &mut self,
        gl: &glow::Context,
        base_size: [u32; 2],
        iterations: usize,
        format: LgPixelFormat,
    ) -> Result<(), LgStatus> {
        if self.base_size == base_size && self.format == format && self.levels.len() == iterations {
            return Ok(());
        }
        unsafe { self.clear(gl) };
        self.base_size = base_size;
        self.format = format;
        let (internal_fmt, data_fmt, data_ty) = gl_format_tuple(format);
        for i in 1..=iterations {
            let w = (base_size[0] >> i).max(1);
            let h = (base_size[1] >> i).max(1);
            let texture = unsafe { gl.create_texture() }.map_err(|_| LgStatus::GlFramebufferError)?;
            unsafe {
                gl.bind_texture(glow::TEXTURE_2D, Some(texture));
                gl.tex_parameter_i32(glow::TEXTURE_2D, glow::TEXTURE_MIN_FILTER, glow::LINEAR as i32);
                gl.tex_parameter_i32(glow::TEXTURE_2D, glow::TEXTURE_MAG_FILTER, glow::LINEAR as i32);
                gl.tex_parameter_i32(glow::TEXTURE_2D, glow::TEXTURE_WRAP_S, glow::CLAMP_TO_EDGE as i32);
                gl.tex_parameter_i32(glow::TEXTURE_2D, glow::TEXTURE_WRAP_T, glow::CLAMP_TO_EDGE as i32);
                gl.tex_image_2d(
                    glow::TEXTURE_2D,
                    0,
                    internal_fmt as i32,
                    w as i32,
                    h as i32,
                    0,
                    data_fmt,
                    data_ty,
                    glow::PixelUnpackData::Slice(None),
                );
            }
            let fbo = unsafe { gl.create_framebuffer() }.map_err(|_| {
                unsafe { gl.delete_texture(texture) };
                LgStatus::GlFramebufferError
            })?;
            unsafe {
                gl.bind_framebuffer(glow::FRAMEBUFFER, Some(fbo));
                gl.framebuffer_texture_2d(
                    glow::FRAMEBUFFER,
                    glow::COLOR_ATTACHMENT0,
                    glow::TEXTURE_2D,
                    Some(texture),
                    0,
                );
                let status = gl.check_framebuffer_status(glow::FRAMEBUFFER);
                if status != glow::FRAMEBUFFER_COMPLETE {
                    gl.delete_framebuffer(fbo);
                    gl.delete_texture(texture);
                    self.clear(gl);
                    return Err(LgStatus::GlFramebufferError);
                }
            }
            self.levels.push(GlBlurLevel { texture, fbo, width: w, height: h });
        }
        Ok(())
    }
}

fn gl_format_tuple(fmt: LgPixelFormat) -> (u32, u32, u32) {
    match fmt {
        LgPixelFormat::Rgba16Float => (glow::RGBA16F, glow::RGBA, glow::HALF_FLOAT),
        LgPixelFormat::Rgba8UnormSrgb => (glow::SRGB8_ALPHA8, glow::RGBA, glow::UNSIGNED_BYTE),
        // Use RGBA8 for both Rgba8Unorm and Bgra8Unorm internal scratch targets (compatible across GL & GLES).
        LgPixelFormat::Rgba8Unorm | LgPixelFormat::Bgra8Unorm => {
            (glow::RGBA8, glow::RGBA, glow::UNSIGNED_BYTE)
        }
    }
}

/// Snapshot of every OpenGL state touched by `GlRenderer`, restored unconditionally before returning.
struct SavedGlState {
    is_gles: bool,
    draw_fbo: i32,
    read_fbo: i32,
    viewport: [i32; 4],
    scissor_enabled: bool,
    scissor_box: [i32; 4],
    blend_enabled: bool,
    blend_src_rgb: i32,
    blend_dst_rgb: i32,
    blend_src_alpha: i32,
    blend_dst_alpha: i32,
    blend_eq_rgb: i32,
    blend_eq_alpha: i32,
    cull_enabled: bool,
    depth_enabled: bool,
    stencil_enabled: bool,
    srgb_enabled: bool,
    color_mask: [bool; 4],
    program: i32,
    vao: i32,
    array_buf: i32,
    ubo_binding: i32,
    active_tex: i32,
    tex0: i32,
    tex1: i32,
    sampler0: i32,
    sampler1: i32,
}

impl SavedGlState {
    unsafe fn capture(gl: &glow::Context, is_gles: bool) -> Self {
        unsafe {
            let draw_fbo = gl.get_parameter_i32(glow::DRAW_FRAMEBUFFER_BINDING);
            let read_fbo = gl.get_parameter_i32(glow::READ_FRAMEBUFFER_BINDING);
            let mut viewport = [0i32; 4];
            gl.get_parameter_i32_slice(glow::VIEWPORT, &mut viewport);
            let scissor_enabled = gl.is_enabled(glow::SCISSOR_TEST);
            let mut scissor_box = [0i32; 4];
            gl.get_parameter_i32_slice(glow::SCISSOR_BOX, &mut scissor_box);
            let blend_enabled = gl.is_enabled(glow::BLEND);
            let blend_src_rgb = gl.get_parameter_i32(glow::BLEND_SRC_RGB);
            let blend_dst_rgb = gl.get_parameter_i32(glow::BLEND_DST_RGB);
            let blend_src_alpha = gl.get_parameter_i32(glow::BLEND_SRC_ALPHA);
            let blend_dst_alpha = gl.get_parameter_i32(glow::BLEND_DST_ALPHA);
            let blend_eq_rgb = gl.get_parameter_i32(glow::BLEND_EQUATION_RGB);
            let blend_eq_alpha = gl.get_parameter_i32(glow::BLEND_EQUATION_ALPHA);
            let cull_enabled = gl.is_enabled(glow::CULL_FACE);
            let depth_enabled = gl.is_enabled(glow::DEPTH_TEST);
            let stencil_enabled = gl.is_enabled(glow::STENCIL_TEST);
            let srgb_enabled = !is_gles && gl.is_enabled(glow::FRAMEBUFFER_SRGB);
            let mut cm = [1i32; 4];
            gl.get_parameter_i32_slice(glow::COLOR_WRITEMASK, &mut cm);
            let color_mask = [cm[0] != 0, cm[1] != 0, cm[2] != 0, cm[3] != 0];
            let program = gl.get_parameter_i32(glow::CURRENT_PROGRAM);
            let vao = gl.get_parameter_i32(glow::VERTEX_ARRAY_BINDING);
            let array_buf = gl.get_parameter_i32(glow::ARRAY_BUFFER_BINDING);
            let ubo_binding = gl.get_parameter_i32(glow::UNIFORM_BUFFER_BINDING);
            let active_tex = gl.get_parameter_i32(glow::ACTIVE_TEXTURE);
            gl.active_texture(glow::TEXTURE0);
            let tex0 = gl.get_parameter_i32(glow::TEXTURE_BINDING_2D);
            let sampler0 = gl.get_parameter_i32(glow::SAMPLER_BINDING);
            gl.active_texture(glow::TEXTURE1);
            let tex1 = gl.get_parameter_i32(glow::TEXTURE_BINDING_2D);
            let sampler1 = gl.get_parameter_i32(glow::SAMPLER_BINDING);
            gl.active_texture(active_tex as u32);

            Self {
                is_gles,
                draw_fbo,
                read_fbo,
                viewport,
                scissor_enabled,
                scissor_box,
                blend_enabled,
                blend_src_rgb,
                blend_dst_rgb,
                blend_src_alpha,
                blend_dst_alpha,
                blend_eq_rgb,
                blend_eq_alpha,
                cull_enabled,
                depth_enabled,
                stencil_enabled,
                srgb_enabled,
                color_mask,
                program,
                vao,
                array_buf,
                ubo_binding,
                active_tex,
                tex0,
                tex1,
                sampler0,
                sampler1,
            }
        }
    }

    unsafe fn restore(&self, gl: &glow::Context) {
        unsafe {
            gl.bind_framebuffer(glow::DRAW_FRAMEBUFFER, to_native_fbo(self.draw_fbo as u32));
            gl.bind_framebuffer(glow::READ_FRAMEBUFFER, to_native_fbo(self.read_fbo as u32));
            gl.viewport(self.viewport[0], self.viewport[1], self.viewport[2], self.viewport[3]);
            set_cap(gl, glow::SCISSOR_TEST, self.scissor_enabled);
            gl.scissor(self.scissor_box[0], self.scissor_box[1], self.scissor_box[2], self.scissor_box[3]);
            set_cap(gl, glow::BLEND, self.blend_enabled);
            gl.blend_func_separate(
                self.blend_src_rgb as u32,
                self.blend_dst_rgb as u32,
                self.blend_src_alpha as u32,
                self.blend_dst_alpha as u32,
            );
            gl.blend_equation_separate(self.blend_eq_rgb as u32, self.blend_eq_alpha as u32);
            set_cap(gl, glow::CULL_FACE, self.cull_enabled);
            set_cap(gl, glow::DEPTH_TEST, self.depth_enabled);
            set_cap(gl, glow::STENCIL_TEST, self.stencil_enabled);
            if !self.is_gles {
                set_cap(gl, glow::FRAMEBUFFER_SRGB, self.srgb_enabled);
            }
            gl.color_mask(self.color_mask[0], self.color_mask[1], self.color_mask[2], self.color_mask[3]);
            gl.use_program(NonZeroU32::new(self.program as u32).map(glow::NativeProgram));
            gl.bind_vertex_array(NonZeroU32::new(self.vao as u32).map(glow::NativeVertexArray));
            gl.bind_buffer(glow::ARRAY_BUFFER, NonZeroU32::new(self.array_buf as u32).map(glow::NativeBuffer));
            gl.bind_buffer(glow::UNIFORM_BUFFER, NonZeroU32::new(self.ubo_binding as u32).map(glow::NativeBuffer));
            gl.active_texture(glow::TEXTURE0);
            gl.bind_texture(glow::TEXTURE_2D, NonZeroU32::new(self.tex0 as u32).map(glow::NativeTexture));
            gl.bind_sampler(0, NonZeroU32::new(self.sampler0 as u32).map(glow::NativeSampler));
            gl.active_texture(glow::TEXTURE1);
            gl.bind_texture(glow::TEXTURE_2D, NonZeroU32::new(self.tex1 as u32).map(glow::NativeTexture));
            gl.bind_sampler(1, NonZeroU32::new(self.sampler1 as u32).map(glow::NativeSampler));
            gl.active_texture(self.active_tex as u32);
        }
    }
}

#[inline]
unsafe fn set_cap(gl: &glow::Context, cap: u32, enabled: bool) {
    unsafe {
        if enabled {
            gl.enable(cap);
        } else {
            gl.disable(cap);
        }
    }
}

#[inline]
fn to_native_fbo(id: u32) -> Option<glow::Framebuffer> {
    NonZeroU32::new(id).map(glow::NativeFramebuffer)
}

#[inline]
fn to_native_tex(id: u32) -> Option<glow::Texture> {
    NonZeroU32::new(id).map(glow::NativeTexture)
}

unsafe fn compile_program(
    gl: &glow::Context,
    vert_src: &str,
    frag_src: &str,
    ubo_names: &[&str],
    tex0_name: &str,
    tex1_name: Option<&str>,
) -> Result<GlProgram, LgStatus> {
    unsafe {
        let vs = gl.create_shader(glow::VERTEX_SHADER).map_err(|_| LgStatus::GlCompileError)?;
        gl.shader_source(vs, vert_src);
        gl.compile_shader(vs);
        if !gl.get_shader_compile_status(vs) {
            gl.delete_shader(vs);
            return Err(LgStatus::GlCompileError);
        }

        let fs = gl.create_shader(glow::FRAGMENT_SHADER).map_err(|_| {
            gl.delete_shader(vs);
            LgStatus::GlCompileError
        })?;
        gl.shader_source(fs, frag_src);
        gl.compile_shader(fs);
        if !gl.get_shader_compile_status(fs) {
            gl.delete_shader(vs);
            gl.delete_shader(fs);
            return Err(LgStatus::GlCompileError);
        }

        let program = gl.create_program().map_err(|_| {
            gl.delete_shader(vs);
            gl.delete_shader(fs);
            LgStatus::GlCompileError
        })?;
        gl.attach_shader(program, vs);
        gl.attach_shader(program, fs);
        gl.link_program(program);
        gl.delete_shader(vs);
        gl.delete_shader(fs);

        if !gl.get_program_link_status(program) {
            gl.delete_program(program);
            return Err(LgStatus::GlCompileError);
        }

        let mut ubo_binding_ok = false;
        for &name in ubo_names {
            if let Some(idx) = gl.get_uniform_block_index(program, name) {
                gl.uniform_block_binding(program, idx, 0);
                ubo_binding_ok = true;
            }
        }

        gl.use_program(Some(program));
        let loc_tex0 = gl.get_uniform_location(program, tex0_name);
        if let Some(loc) = loc_tex0.as_ref() {
            gl.uniform_1_i32(Some(loc), 0);
        }
        let loc_tex1 = tex1_name.and_then(|n| gl.get_uniform_location(program, n));
        if let Some(loc) = loc_tex1.as_ref() {
            gl.uniform_1_i32(Some(loc), 1);
        }

        Ok(GlProgram { program, ubo_binding_ok, loc_tex0, loc_tex1 })
    }
}

/// Zero-copy OpenGL / OpenGL ES Liquid Glass renderer borrowing the host's active GL context.
pub struct GlRenderer {
    gl: glow::Context,
    is_gles: bool,
    prog_down: GlProgram,
    prog_up: GlProgram,
    prog_glass: GlProgram,
    sampler: glow::Sampler,
    empty_vao: glow::VertexArray,
    glass_vao: glow::VertexArray,
    vbo: glow::Buffer,
    vbo_capacity: usize,
    kawase_ubo: glow::Buffer,
    glass_ubo: glow::Buffer,
    scratch_verts: Vec<[f32; 2]>,
    blur_pool: GlBlurPool,
}

impl GlRenderer {
    /// Initializes the renderer on the currently active OpenGL / OpenGL ES context.
    ///
    /// # Safety
    /// A valid OpenGL 3.3+ or OpenGL ES 3.0+ context must be current on the calling thread, and
    /// `loader` must resolve valid GL function pointers for that context.
    pub unsafe fn new<F>(loader: F) -> Result<Self, LgStatus>
    where
        F: FnMut(&str) -> *const core::ffi::c_void,
    {
        let gl = unsafe { glow::Context::from_loader_function(loader) };
        let version_str = unsafe { gl.get_parameter_string(glow::VERSION) };
        let is_gles = version_str.contains("OpenGL ES");

        let saved = unsafe { SavedGlState::capture(&gl, is_gles) };
        let init_res = unsafe { Self::init_resources(&gl, is_gles) };
        unsafe { saved.restore(&gl) };
        let (prog_down, prog_up, prog_glass, sampler, empty_vao, glass_vao, vbo, vbo_capacity, kawase_ubo, glass_ubo) =
            init_res?;

        Ok(Self {
            gl,
            is_gles,
            prog_down,
            prog_up,
            prog_glass,
            sampler,
            empty_vao,
            glass_vao,
            vbo,
            vbo_capacity,
            kawase_ubo,
            glass_ubo,
            scratch_verts: Vec::with_capacity(vbo_capacity),
            blur_pool: GlBlurPool::new(),
        })
    }

    #[allow(clippy::type_complexity)]
    unsafe fn init_resources(
        gl: &glow::Context,
        is_gles: bool,
    ) -> Result<
        (
            GlProgram,
            GlProgram,
            GlProgram,
            glow::Sampler,
            glow::VertexArray,
            glow::VertexArray,
            glow::Buffer,
            usize,
            glow::Buffer,
            glow::Buffer,
        ),
        LgStatus,
    > {
        let (kd_v, kd_f, ku_v, ku_f, lg_v, lg_f) = if is_gles {
            (
                gles300::KAWASE_DOWN_VERT,
                gles300::KAWASE_DOWN_FRAG,
                gles300::KAWASE_UP_VERT,
                gles300::KAWASE_UP_FRAG,
                gles300::LIQUIDGLASS_VERT,
                gles300::LIQUIDGLASS_FRAG,
            )
        } else {
            (
                glsl330::KAWASE_DOWN_VERT,
                glsl330::KAWASE_DOWN_FRAG,
                glsl330::KAWASE_UP_VERT,
                glsl330::KAWASE_UP_FRAG,
                glsl330::LIQUIDGLASS_VERT,
                glsl330::LIQUIDGLASS_FRAG,
            )
        };

        let prog_down = unsafe {
            compile_program(
                gl,
                kd_v,
                kd_f,
                &["KawaseUniforms_block_0Vertex", "KawaseUniforms_block_0Fragment"],
                "_group_0_binding_1_fs",
                None,
            )?
        };
        let prog_up = unsafe {
            compile_program(
                gl,
                ku_v,
                ku_f,
                &["KawaseUniforms_block_0Vertex", "KawaseUniforms_block_0Fragment"],
                "_group_0_binding_1_fs",
                None,
            )
        };
        let prog_up = match prog_up {
            Ok(p) => p,
            Err(e) => {
                unsafe { gl.delete_program(prog_down.program) };
                return Err(e);
            }
        };
        let prog_glass = unsafe {
            compile_program(
                gl,
                lg_v,
                lg_f,
                &["GlassUniforms_block_0Vertex", "GlassUniforms_block_0Fragment"],
                "_group_0_binding_1_fs",
                Some("_group_0_binding_3_fs"),
            )
        };
        let prog_glass = match prog_glass {
            Ok(p) => p,
            Err(e) => {
                unsafe {
                    gl.delete_program(prog_down.program);
                    gl.delete_program(prog_up.program);
                }
                return Err(e);
            }
        };
        if !prog_down.ubo_binding_ok || !prog_up.ubo_binding_ok || !prog_glass.ubo_binding_ok {
            unsafe {
                gl.delete_program(prog_down.program);
                gl.delete_program(prog_up.program);
                gl.delete_program(prog_glass.program);
            }
            return Err(LgStatus::GlCompileError);
        }

        // Dedicated linear+clamp sampler object overrides default mipmap-linear filter on borrowed
        // host textures without mutating any host texture parameters.
        let sampler = unsafe { gl.create_sampler() }.map_err(|_| LgStatus::GlCompileError)?;
        unsafe {
            gl.sampler_parameter_i32(sampler, glow::TEXTURE_MIN_FILTER, glow::LINEAR as i32);
            gl.sampler_parameter_i32(sampler, glow::TEXTURE_MAG_FILTER, glow::LINEAR as i32);
            gl.sampler_parameter_i32(sampler, glow::TEXTURE_WRAP_S, glow::CLAMP_TO_EDGE as i32);
            gl.sampler_parameter_i32(sampler, glow::TEXTURE_WRAP_T, glow::CLAMP_TO_EDGE as i32);
        }

        let empty_vao = unsafe { gl.create_vertex_array() }.map_err(|_| LgStatus::GlCompileError)?;
        let glass_vao = unsafe { gl.create_vertex_array() }.map_err(|_| LgStatus::GlCompileError)?;
        let vbo = unsafe { gl.create_buffer() }.map_err(|_| LgStatus::GlCompileError)?;
        let kawase_ubo = unsafe { gl.create_buffer() }.map_err(|_| LgStatus::GlCompileError)?;
        let glass_ubo = unsafe { gl.create_buffer() }.map_err(|_| LgStatus::GlCompileError)?;

        let vbo_capacity = 256usize;
        unsafe {
            gl.bind_vertex_array(Some(glass_vao));
            gl.bind_buffer(glow::ARRAY_BUFFER, Some(vbo));
            gl.buffer_data_size(
                glow::ARRAY_BUFFER,
                (vbo_capacity * core::mem::size_of::<[f32; 2]>()) as i32,
                glow::STREAM_DRAW,
            );
            gl.enable_vertex_attrib_array(0);
            gl.vertex_attrib_pointer_f32(
                0,
                2,
                glow::FLOAT,
                false,
                core::mem::size_of::<[f32; 2]>() as i32,
                0,
            );

            gl.bind_buffer(glow::UNIFORM_BUFFER, Some(kawase_ubo));
            gl.buffer_data_size(
                glow::UNIFORM_BUFFER,
                core::mem::size_of::<KawaseUniforms>() as i32,
                glow::STREAM_DRAW,
            );
            gl.bind_buffer(glow::UNIFORM_BUFFER, Some(glass_ubo));
            gl.buffer_data_size(
                glow::UNIFORM_BUFFER,
                core::mem::size_of::<GlassUniforms>() as i32,
                glow::STREAM_DRAW,
            );
        }

        let _ = (&prog_down.loc_tex0, &prog_up.loc_tex0, &prog_glass.loc_tex0, &prog_glass.loc_tex1);
        Ok((prog_down, prog_up, prog_glass, sampler, empty_vao, glass_vao, vbo, vbo_capacity, kawase_ubo, glass_ubo))
    }

    /// Releases pooled internal scratch blur textures/FBOs (e.g. when a display output is removed).
    ///
    /// # Safety
    /// The OpenGL context used to create this renderer must be current.
    pub unsafe fn invalidate_caches(&mut self) {
        let saved = unsafe { SavedGlState::capture(&self.gl, self.is_gles) };
        unsafe { self.blur_pool.clear(&self.gl) };
        unsafe { saved.restore(&self.gl) };
    }

    /// Renders a batch of glass shapes from a borrowed backdrop texture into a borrowed target FBO.
    /// Zero per-frame heap allocations once steady-state; strictly saves & restores all GL state.
    ///
    /// # Safety
    /// The OpenGL context used to create this renderer must be current, and all pointers/handles
    /// inside `params` must be valid for the duration of the call.
    pub unsafe fn render(&mut self, params: &LgRenderParams) -> LgStatus {
        if params.shape_count == 0 {
            return LgStatus::Ok;
        }
        if params.shapes.is_null() {
            return LgStatus::NullPointer;
        }
        let backdrop = &params.backdrop;
        let target = &params.target;
        if backdrop.texture_id == 0 || backdrop.width == 0 || backdrop.height == 0 {
            return LgStatus::InvalidArgument;
        }
        let shapes = unsafe { core::slice::from_raw_parts(params.shapes, params.shape_count as usize) };
        let clips: &[LgRect] = if target.clip_rect_count > 0 && !target.clip_rects.is_null() {
            unsafe { core::slice::from_raw_parts(target.clip_rects, target.clip_rect_count as usize) }
        } else {
            &[]
        };

        let saved = unsafe { SavedGlState::capture(&self.gl, self.is_gles) };
        let status = unsafe { self.render_inner(shapes, clips, params, &saved) };
        unsafe { saved.restore(&self.gl) };
        status
    }

    unsafe fn render_inner(
        &mut self,
        shapes: &[LgShape],
        clips: &[LgRect],
        params: &LgRenderParams,
        guard: &SavedGlState,
    ) -> LgStatus {
        let gl = &self.gl;
        let backdrop = &params.backdrop;
        let target = &params.target;

        // Clean baseline render state for offscreen & composite passes.
        unsafe {
            gl.disable(glow::DEPTH_TEST);
            gl.disable(glow::STENCIL_TEST);
            gl.disable(glow::CULL_FACE);
            gl.disable(glow::SCISSOR_TEST);
            if !self.is_gles {
                gl.disable(glow::FRAMEBUFFER_SRGB);
            }
            gl.color_mask(true, true, true, true);
            gl.bind_sampler(0, Some(self.sampler));
            gl.bind_sampler(1, Some(self.sampler));
        }

        let mut max_strength = 0u32;
        let mut max_scale = 1.0f32;
        for s in shapes {
            max_strength = max_strength.max(strength_for_frost(s.material.frost));
            max_scale = max_scale.max(s.scale);
        }
        let step = kawase_for_strength(max_strength, max_scale);
        if let Err(err) = unsafe {
            self.blur_pool.ensure(gl, [backdrop.width, backdrop.height], step.iterations, backdrop.format)
        } {
            return err;
        }

        let sharp_tex = to_native_tex(backdrop.texture_id);
        let use_blur = !self.blur_pool.levels.is_empty();

        // 1. Dual Kawase blur chain into pooled internal FBOs (no blending, fullscreen triangle).
        if use_blur {
            unsafe {
                gl.disable(glow::BLEND);
                gl.bind_vertex_array(Some(self.empty_vao));
                gl.bind_buffer_base(glow::UNIFORM_BUFFER, 0, Some(self.kawase_ubo));
                gl.active_texture(glow::TEXTURE0);

                // Downsample chain
                gl.use_program(Some(self.prog_down.program));
                let n = self.blur_pool.levels.len();
                for i in 0..n {
                    let (src_tex, sw, sh) = if i == 0 {
                        (sharp_tex, backdrop.width, backdrop.height)
                    } else {
                        let prev = &self.blur_pool.levels[i - 1];
                        (Some(prev.texture), prev.width, prev.height)
                    };
                    let dst = &self.blur_pool.levels[i];
                    let ubo = KawaseUniforms {
                        halfpixel: [0.5 / sw.max(1) as f32, 0.5 / sh.max(1) as f32],
                        offset: step.offset,
                        flip_y: if i == 0 && backdrop.y_origin == LgYOrigin::TopLeft { 1.0 } else { 0.0 },
                    };
                    gl.bind_buffer(glow::UNIFORM_BUFFER, Some(self.kawase_ubo));
                    gl.buffer_sub_data_u8_slice(glow::UNIFORM_BUFFER, 0, bytemuck::bytes_of(&ubo));
                    gl.bind_texture(glow::TEXTURE_2D, src_tex);
                    gl.bind_framebuffer(glow::FRAMEBUFFER, Some(dst.fbo));
                    gl.viewport(0, 0, dst.width as i32, dst.height as i32);
                    gl.draw_arrays(glow::TRIANGLES, 0, 3);
                }

                // Upsample chain back to level 0 (half-resolution)
                gl.use_program(Some(self.prog_up.program));
                for i in (1..n).rev() {
                    let src = &self.blur_pool.levels[i];
                    let dst = &self.blur_pool.levels[i - 1];
                    let ubo = KawaseUniforms {
                        halfpixel: [0.5 / src.width.max(1) as f32, 0.5 / src.height.max(1) as f32],
                        offset: step.offset,
                        flip_y: 0.0,
                    };
                    gl.bind_buffer(glow::UNIFORM_BUFFER, Some(self.kawase_ubo));
                    gl.buffer_sub_data_u8_slice(glow::UNIFORM_BUFFER, 0, bytemuck::bytes_of(&ubo));
                    gl.bind_texture(glow::TEXTURE_2D, Some(src.texture));
                    gl.bind_framebuffer(glow::FRAMEBUFFER, Some(dst.fbo));
                    gl.viewport(0, 0, dst.width as i32, dst.height as i32);
                    gl.draw_arrays(glow::TRIANGLES, 0, 3);
                }
            }
        }

        // 2. Liquid Glass composite pass directly into caller-provided target FBO.
        let (blur_tex, blur_size) = if use_blur {
            let l0 = &self.blur_pool.levels[0];
            (Some(l0.texture), [l0.width, l0.height])
        } else {
            (sharp_tex, [backdrop.width, backdrop.height])
        };

        unsafe {
            gl.bind_framebuffer(glow::DRAW_FRAMEBUFFER, to_native_fbo(target.fbo_id));
            if target.viewport.width > 0 && target.viewport.height > 0 {
                gl.viewport(
                    target.viewport.x,
                    target.viewport.y,
                    target.viewport.width,
                    target.viewport.height,
                );
            } else {
                gl.viewport(guard.viewport[0], guard.viewport[1], guard.viewport[2], guard.viewport[3]);
            }

            gl.enable(glow::BLEND);
            if target.premultiplied_output != 0 {
                gl.blend_func(glow::ONE, glow::ONE_MINUS_SRC_ALPHA);
            } else {
                gl.blend_func(glow::SRC_ALPHA, glow::ONE_MINUS_SRC_ALPHA);
            }
            gl.blend_equation(glow::FUNC_ADD);

            gl.use_program(Some(self.prog_glass.program));
            gl.bind_vertex_array(Some(self.glass_vao));
            gl.bind_buffer_base(glow::UNIFORM_BUFFER, 0, Some(self.glass_ubo));
            gl.active_texture(glow::TEXTURE0);
            gl.bind_texture(glow::TEXTURE_2D, sharp_tex);
            gl.active_texture(glow::TEXTURE1);
            gl.bind_texture(glow::TEXTURE_2D, blur_tex);

            let bounds = LgRect::new(0.0, 0.0, backdrop.width as f32, backdrop.height as f32);
            for shape in shapes {
                let margin = shape.material.draw_margin() * shape.scale.max(0.01);
                let draw_rect = shape.rect.expanded(margin).intersected(bounds);
                self.scratch_verts.clear();
                push_clipped_quad_verts(&mut self.scratch_verts, draw_rect, clips);
                if self.scratch_verts.is_empty() {
                    continue;
                }

                gl.bind_buffer(glow::ARRAY_BUFFER, Some(self.vbo));
                if self.scratch_verts.len() > self.vbo_capacity {
                    self.vbo_capacity = self.scratch_verts.len().next_power_of_two();
                    gl.buffer_data_size(
                        glow::ARRAY_BUFFER,
                        (self.vbo_capacity * core::mem::size_of::<[f32; 2]>()) as i32,
                        glow::STREAM_DRAW,
                    );
                }
                gl.buffer_sub_data_u8_slice(
                    glow::ARRAY_BUFFER,
                    0,
                    bytemuck::cast_slice(&self.scratch_verts),
                );

                let ubo = GlassUniforms::pack(shape, backdrop, target, use_blur, blur_size, step.offset);
                gl.bind_buffer(glow::UNIFORM_BUFFER, Some(self.glass_ubo));
                gl.buffer_sub_data_u8_slice(glow::UNIFORM_BUFFER, 0, bytemuck::bytes_of(&ubo));

                gl.draw_arrays(glow::TRIANGLES, 0, self.scratch_verts.len() as i32);
            }
        }

        LgStatus::Ok
    }

    /// Explicitly destroys all owned GL objects while the host GL context is current.
    ///
    /// # Safety
    /// The OpenGL context used to create this renderer must be current.
    pub unsafe fn destroy(mut self) {
        unsafe {
            self.blur_pool.clear(&self.gl);
            self.gl.delete_sampler(self.sampler);
            self.gl.delete_program(self.prog_down.program);
            self.gl.delete_program(self.prog_up.program);
            self.gl.delete_program(self.prog_glass.program);
            self.gl.delete_vertex_array(self.empty_vao);
            self.gl.delete_vertex_array(self.glass_vao);
            self.gl.delete_buffer(self.vbo);
            self.gl.delete_buffer(self.kawase_ubo);
            self.gl.delete_buffer(self.glass_ubo);
        }
    }
}

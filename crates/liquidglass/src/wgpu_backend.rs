// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Cross-platform `wgpu` backend (Vulkan / Metal / DX12 / GLES / WebGPU).
// Supports zero-copy borrowed `TextureView` -> `TextureView` rendering, pooled internal scratch
// Dual-Kawase blur textures (zero per-frame allocations), headless GPU creation for millisecond
// visual regression testing, and an optional CPU `&[u8]` -> `&mut [u8]` buffer helper.

use std::sync::Arc;

use crate::core::{
    expand_for_iterations, kawase_for_strength, push_clipped_quad_verts, strength_for_frost,
    GlassUniforms, KawaseUniforms, LgBackdropDesc, LgPixelFormat, LgRect, LgShape, LgTargetDesc,
    LgYOrigin,
};
use crate::shaders::wgsl as wgsl_src;

fn to_wgpu_format(fmt: LgPixelFormat) -> wgpu::TextureFormat {
    match fmt {
        LgPixelFormat::Rgba8Unorm => wgpu::TextureFormat::Rgba8Unorm,
        LgPixelFormat::Bgra8Unorm => wgpu::TextureFormat::Bgra8Unorm,
        LgPixelFormat::Rgba16Float => wgpu::TextureFormat::Rgba16Float,
        LgPixelFormat::Rgba8UnormSrgb => wgpu::TextureFormat::Rgba8UnormSrgb,
    }
}

struct BlurLevel {
    _texture: wgpu::Texture,
    view: wgpu::TextureView,
    width: u32,
    height: u32,
}

struct BlurPool {
    base_size: [u32; 2],
    format: wgpu::TextureFormat,
    levels: Vec<BlurLevel>,
}

impl BlurPool {
    fn new() -> Self {
        Self {
            base_size: [0, 0],
            format: wgpu::TextureFormat::Rgba8Unorm,
            levels: Vec::new(),
        }
    }

    fn ensure(
        &mut self,
        device: &wgpu::Device,
        base_size: [u32; 2],
        iterations: usize,
        format: wgpu::TextureFormat,
    ) {
        if self.base_size == base_size && self.format == format && self.levels.len() == iterations {
            return;
        }
        self.levels.clear();
        self.base_size = base_size;
        self.format = format;
        for i in 1..=iterations {
            let w = (base_size[0] >> i).max(1);
            let h = (base_size[1] >> i).max(1);
            let tex = device.create_texture(&wgpu::TextureDescriptor {
                label: Some("lg_kawase_level"),
                size: wgpu::Extent3d { width: w, height: h, depth_or_array_layers: 1 },
                mip_level_count: 1,
                sample_count: 1,
                dimension: wgpu::TextureDimension::D2,
                format,
                usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::TEXTURE_BINDING,
                view_formats: &[],
            });
            let view = tex.create_view(&wgpu::TextureViewDescriptor::default());
            self.levels.push(BlurLevel { _texture: tex, view, width: w, height: h });
        }
    }
}

/// Cross-platform zero-copy Liquid Glass renderer backed by `wgpu`.
pub struct WgpuRenderer {
    device: Arc<wgpu::Device>,
    queue: Arc<wgpu::Queue>,
    sampler: wgpu::Sampler,
    kawase_bgl: wgpu::BindGroupLayout,
    glass_bgl: wgpu::BindGroupLayout,
    kawase_down_shader: wgpu::ShaderModule,
    kawase_up_shader: wgpu::ShaderModule,
    glass_shader: wgpu::ShaderModule,
    kawase_pipeline_layout: wgpu::PipelineLayout,
    glass_pipeline_layout: wgpu::PipelineLayout,
    cached_format: Option<wgpu::TextureFormat>,
    kawase_down_pipeline: Option<wgpu::RenderPipeline>,
    kawase_up_pipeline: Option<wgpu::RenderPipeline>,
    glass_pipeline: Option<wgpu::RenderPipeline>,
    kawase_uniform_buf: wgpu::Buffer,
    glass_uniform_buf: wgpu::Buffer,
    vertex_buf: wgpu::Buffer,
    vertex_capacity: usize,
    scratch_verts: Vec<[f32; 2]>,
    blur_pool: BlurPool,
}

impl WgpuRenderer {
    /// Creates a renderer bound to a host-owned `wgpu::Device` and `wgpu::Queue`.
    pub fn new(device: Arc<wgpu::Device>, queue: Arc<wgpu::Queue>) -> Self {
        let sampler = device.create_sampler(&wgpu::SamplerDescriptor {
            label: Some("lg_linear_clamp"),
            address_mode_u: wgpu::AddressMode::ClampToEdge,
            address_mode_v: wgpu::AddressMode::ClampToEdge,
            address_mode_w: wgpu::AddressMode::ClampToEdge,
            mag_filter: wgpu::FilterMode::Linear,
            min_filter: wgpu::FilterMode::Linear,
            mipmap_filter: wgpu::MipmapFilterMode::Linear,
            ..Default::default()
        });

        let kawase_bgl = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("lg_kawase_bgl"),
            entries: &[
                wgpu::BindGroupLayoutEntry {
                    binding: 0,
                    visibility: wgpu::ShaderStages::VERTEX_FRAGMENT,
                    ty: wgpu::BindingType::Buffer {
                        ty: wgpu::BufferBindingType::Uniform,
                        has_dynamic_offset: false,
                        min_binding_size: None,
                    },
                    count: None,
                },
                wgpu::BindGroupLayoutEntry {
                    binding: 1,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Float { filterable: true },
                        view_dimension: wgpu::TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                },
                wgpu::BindGroupLayoutEntry {
                    binding: 2,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Sampler(wgpu::SamplerBindingType::Filtering),
                    count: None,
                },
            ],
        });

        let glass_bgl = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("lg_glass_bgl"),
            entries: &[
                wgpu::BindGroupLayoutEntry {
                    binding: 0,
                    visibility: wgpu::ShaderStages::VERTEX_FRAGMENT,
                    ty: wgpu::BindingType::Buffer {
                        ty: wgpu::BufferBindingType::Uniform,
                        has_dynamic_offset: false,
                        min_binding_size: None,
                    },
                    count: None,
                },
                wgpu::BindGroupLayoutEntry {
                    binding: 1,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Float { filterable: true },
                        view_dimension: wgpu::TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                },
                wgpu::BindGroupLayoutEntry {
                    binding: 2,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Sampler(wgpu::SamplerBindingType::Filtering),
                    count: None,
                },
                wgpu::BindGroupLayoutEntry {
                    binding: 3,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Float { filterable: true },
                        view_dimension: wgpu::TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                },
                wgpu::BindGroupLayoutEntry {
                    binding: 4,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Sampler(wgpu::SamplerBindingType::Filtering),
                    count: None,
                },
            ],
        });

        let kawase_down_shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("lg_kawase_down"),
            source: wgpu::ShaderSource::Wgsl(wgsl_src::KAWASE_DOWN.into()),
        });
        let kawase_up_shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("lg_kawase_up"),
            source: wgpu::ShaderSource::Wgsl(wgsl_src::KAWASE_UP.into()),
        });
        let glass_shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("lg_liquidglass"),
            source: wgpu::ShaderSource::Wgsl(wgsl_src::LIQUIDGLASS.into()),
        });

        let kawase_pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("lg_kawase_pl"),
            bind_group_layouts: &[Some(&kawase_bgl)],
            immediate_size: 0,
        });
        let glass_pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("lg_glass_pl"),
            bind_group_layouts: &[Some(&glass_bgl)],
            immediate_size: 0,
        });

        let kawase_uniform_buf = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("lg_kawase_ubo"),
            size: core::mem::size_of::<KawaseUniforms>() as u64,
            usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });
        let glass_uniform_buf = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("lg_glass_ubo"),
            size: core::mem::size_of::<GlassUniforms>() as u64,
            usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });

        let vertex_capacity = 256usize;
        let vertex_buf = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("lg_vbo"),
            size: (vertex_capacity * core::mem::size_of::<[f32; 2]>()) as u64,
            usage: wgpu::BufferUsages::VERTEX | wgpu::BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });

        Self {
            device,
            queue,
            sampler,
            kawase_bgl,
            glass_bgl,
            kawase_down_shader,
            kawase_up_shader,
            glass_shader,
            kawase_pipeline_layout,
            glass_pipeline_layout,
            cached_format: None,
            kawase_down_pipeline: None,
            kawase_up_pipeline: None,
            glass_pipeline: None,
            kawase_uniform_buf,
            glass_uniform_buf,
            vertex_buf,
            vertex_capacity,
            scratch_verts: Vec::with_capacity(vertex_capacity),
            blur_pool: BlurPool::new(),
        }
    }

    /// Creates a headless GPU renderer (Vulkan / Metal / DX12 / GL) for tests and offline rendering.
    pub fn new_headless() -> Result<Self, String> {
        let instance = wgpu::Instance::new(wgpu::InstanceDescriptor::new_without_display_handle());
        let adapter = pollster::block_on(instance.request_adapter(&wgpu::RequestAdapterOptions {
            power_preference: wgpu::PowerPreference::HighPerformance,
            compatible_surface: None,
            force_fallback_adapter: false,
            ..Default::default()
        }))
        .map_err(|e| format!("wgpu request_adapter failed: {e}"))?;

        let (device, queue) = pollster::block_on(adapter.request_device(&wgpu::DeviceDescriptor {
            label: Some("lg_headless_device"),
            ..Default::default()
        }))
        .map_err(|e| format!("wgpu request_device failed: {e}"))?;

        Ok(Self::new(Arc::new(device), Arc::new(queue)))
    }

    pub fn device(&self) -> &wgpu::Device {
        &self.device
    }

    pub fn queue(&self) -> &wgpu::Queue {
        &self.queue
    }

    fn ensure_pipelines(&mut self, format: wgpu::TextureFormat) {
        if self.cached_format == Some(format) {
            return;
        }
        let make_kawase = |label: &str, module: &wgpu::ShaderModule| {
            self.device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
                label: Some(label),
                layout: Some(&self.kawase_pipeline_layout),
                vertex: wgpu::VertexState {
                    module,
                    entry_point: Some("vs_main"),
                    compilation_options: wgpu::PipelineCompilationOptions::default(),
                    buffers: &[],
                },
                fragment: Some(wgpu::FragmentState {
                    module,
                    entry_point: Some("fs_main"),
                    compilation_options: wgpu::PipelineCompilationOptions::default(),
                    targets: &[Some(wgpu::ColorTargetState {
                        format,
                        blend: None,
                        write_mask: wgpu::ColorWrites::ALL,
                    })],
                }),
                primitive: wgpu::PrimitiveState::default(),
                depth_stencil: None,
                multisample: wgpu::MultisampleState::default(),
                multiview_mask: None,
                cache: None,
            })
        };

        self.kawase_down_pipeline = Some(make_kawase("lg_kawase_down_pipe", &self.kawase_down_shader));
        self.kawase_up_pipeline = Some(make_kawase("lg_kawase_up_pipe", &self.kawase_up_shader));
        self.glass_pipeline = Some(self.device.create_render_pipeline(
            &wgpu::RenderPipelineDescriptor {
                label: Some("lg_glass_pipe"),
                layout: Some(&self.glass_pipeline_layout),
                vertex: wgpu::VertexState {
                    module: &self.glass_shader,
                    entry_point: Some("vs_main"),
                    compilation_options: wgpu::PipelineCompilationOptions::default(),
                    buffers: &[Some(wgpu::VertexBufferLayout {
                        array_stride: core::mem::size_of::<[f32; 2]>() as u64,
                        step_mode: wgpu::VertexStepMode::Vertex,
                        attributes: &wgpu::vertex_attr_array![0 => Float32x2],
                    })],
                },
                fragment: Some(wgpu::FragmentState {
                    module: &self.glass_shader,
                    entry_point: Some("fs_main"),
                    compilation_options: wgpu::PipelineCompilationOptions::default(),
                    targets: &[Some(wgpu::ColorTargetState {
                        format,
                        blend: Some(wgpu::BlendState::PREMULTIPLIED_ALPHA_BLENDING),
                        write_mask: wgpu::ColorWrites::ALL,
                    })],
                }),
                primitive: wgpu::PrimitiveState::default(),
                depth_stencil: None,
                multisample: wgpu::MultisampleState::default(),
                multiview_mask: None,
                cache: None,
            },
        ));
        self.cached_format = Some(format);
    }

    fn run_kawase_step(
        &self,
        encoder: &mut wgpu::CommandEncoder,
        pipeline: &wgpu::RenderPipeline,
        src_view: &wgpu::TextureView,
        dst_view: &wgpu::TextureView,
        src_w: u32,
        src_h: u32,
        offset: f32,
    ) {
        let ubo = KawaseUniforms {
            halfpixel: [0.5 / src_w.max(1) as f32, 0.5 / src_h.max(1) as f32],
            offset,
            flip_y: 1.0, // wgpu textures have TopLeft origin; flip NDC-derived uv.y
        };
        self.queue.write_buffer(&self.kawase_uniform_buf, 0, bytemuck::bytes_of(&ubo));

        let bg = self.device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("lg_kawase_bg"),
            layout: &self.kawase_bgl,
            entries: &[
                wgpu::BindGroupEntry { binding: 0, resource: self.kawase_uniform_buf.as_entire_binding() },
                wgpu::BindGroupEntry { binding: 1, resource: wgpu::BindingResource::TextureView(src_view) },
                wgpu::BindGroupEntry { binding: 2, resource: wgpu::BindingResource::Sampler(&self.sampler) },
            ],
        });

        let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
            label: Some("lg_kawase_pass"),
            color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                view: dst_view,
                depth_slice: None,
                resolve_target: None,
                ops: wgpu::Operations { load: wgpu::LoadOp::Load, store: wgpu::StoreOp::Store },
            })],
            depth_stencil_attachment: None,
            timestamp_writes: None,
            occlusion_query_set: None,
            multiview_mask: None,
        });
        pass.set_pipeline(pipeline);
        pass.set_bind_group(0, &bg, &[]);
        pass.draw(0..3, 0..1);
    }

    /// Zero-copy GPU render: borrows host `backdrop_view` and composites all `shapes` into `target_view`.
    pub fn render_borrowed(
        &mut self,
        backdrop_view: &wgpu::TextureView,
        backdrop: &LgBackdropDesc,
        target_view: &wgpu::TextureView,
        target: &LgTargetDesc,
        shapes: &[LgShape],
        clips: &[LgRect],
    ) {
        if shapes.is_empty() || backdrop.width == 0 || backdrop.height == 0 {
            return;
        }
        let format = to_wgpu_format(target.format);
        self.ensure_pipelines(format);

        // Determine max frost strength across the batch.
        let mut max_strength = 0u32;
        let mut max_scale = 1.0f32;
        for s in shapes {
            max_strength = max_strength.max(strength_for_frost(s.material.frost));
            max_scale = max_scale.max(s.scale);
        }
        let step = kawase_for_strength(max_strength, max_scale);
        self.blur_pool.ensure(&self.device, [backdrop.width, backdrop.height], step.iterations, format);

        // Execute Dual Kawase downsample + upsample chain down to level 1 (each step submits in order
        // because we reuse one uniform buffer; with <= 4 levels that's tiny and zero-alloc).
        let use_blur = !self.blur_pool.levels.is_empty();
        if use_blur {
            let down_pipe = self.kawase_down_pipeline.as_ref().unwrap();
            let up_pipe = self.kawase_up_pipeline.as_ref().unwrap();
            let n = self.blur_pool.levels.len();
            for i in 0..n {
                let mut enc = self.device.create_command_encoder(&wgpu::CommandEncoderDescriptor::default());
                let (src_v, sw, sh) = if i == 0 {
                    (backdrop_view, backdrop.width, backdrop.height)
                } else {
                    let prev = &self.blur_pool.levels[i - 1];
                    (&prev.view, prev.width, prev.height)
                };
                self.run_kawase_step(&mut enc, down_pipe, src_v, &self.blur_pool.levels[i].view, sw, sh, step.offset);
                self.queue.submit(Some(enc.finish()));
            }
            for i in (1..n).rev() {
                let mut enc = self.device.create_command_encoder(&wgpu::CommandEncoderDescriptor::default());
                let src = &self.blur_pool.levels[i];
                let dst = &self.blur_pool.levels[i - 1];
                self.run_kawase_step(&mut enc, up_pipe, &src.view, &dst.view, src.width, src.height, step.offset);
                self.queue.submit(Some(enc.finish()));
            }
        }

        let blur_view = if use_blur { &self.blur_pool.levels[0].view } else { backdrop_view };
        let blur_size = if use_blur {
            [self.blur_pool.levels[0].width, self.blur_pool.levels[0].height]
        } else {
            [backdrop.width, backdrop.height]
        };

        let glass_pipe = self.glass_pipeline.as_ref().unwrap();
        let bounds = LgRect::new(0.0, 0.0, backdrop.width as f32, backdrop.height as f32);

        for shape in shapes {
            let margin = shape.material.draw_margin() * shape.scale.max(0.01);
            let draw_rect = shape.rect.expanded(margin).intersected(bounds);
            self.scratch_verts.clear();
            push_clipped_quad_verts(&mut self.scratch_verts, draw_rect, clips);
            if self.scratch_verts.is_empty() {
                continue;
            }
            if self.scratch_verts.len() > self.vertex_capacity {
                self.vertex_capacity = self.scratch_verts.len().next_power_of_two();
                self.vertex_buf = self.device.create_buffer(&wgpu::BufferDescriptor {
                    label: Some("lg_vbo"),
                    size: (self.vertex_capacity * core::mem::size_of::<[f32; 2]>()) as u64,
                    usage: wgpu::BufferUsages::VERTEX | wgpu::BufferUsages::COPY_DST,
                    mapped_at_creation: false,
                });
            }
            self.queue.write_buffer(&self.vertex_buf, 0, bytemuck::cast_slice(&self.scratch_verts));

            let ubo = GlassUniforms::pack(shape, backdrop, target, use_blur, blur_size, step.offset);
            self.queue.write_buffer(&self.glass_uniform_buf, 0, bytemuck::bytes_of(&ubo));

            let bg = self.device.create_bind_group(&wgpu::BindGroupDescriptor {
                label: Some("lg_glass_bg"),
                layout: &self.glass_bgl,
                entries: &[
                    wgpu::BindGroupEntry { binding: 0, resource: self.glass_uniform_buf.as_entire_binding() },
                    wgpu::BindGroupEntry { binding: 1, resource: wgpu::BindingResource::TextureView(backdrop_view) },
                    wgpu::BindGroupEntry { binding: 2, resource: wgpu::BindingResource::Sampler(&self.sampler) },
                    wgpu::BindGroupEntry { binding: 3, resource: wgpu::BindingResource::TextureView(blur_view) },
                    wgpu::BindGroupEntry { binding: 4, resource: wgpu::BindingResource::Sampler(&self.sampler) },
                ],
            });

            let mut enc = self.device.create_command_encoder(&wgpu::CommandEncoderDescriptor::default());
            {
                let mut pass = enc.begin_render_pass(&wgpu::RenderPassDescriptor {
                    label: Some("lg_glass_pass"),
                    color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                        view: target_view,
                        depth_slice: None,
                        resolve_target: None,
                        ops: wgpu::Operations { load: wgpu::LoadOp::Load, store: wgpu::StoreOp::Store },
                    })],
                    depth_stencil_attachment: None,
                    timestamp_writes: None,
                    occlusion_query_set: None,
                    multiview_mask: None,
                });
                pass.set_pipeline(glass_pipe);
                pass.set_bind_group(0, &bg, &[]);
                pass.set_vertex_buffer(0, self.vertex_buf.slice(..));
                pass.draw(0..self.scratch_verts.len() as u32, 0..1);
            }
            self.queue.submit(Some(enc.finish()));
        }

        let _ = expand_for_iterations(step.iterations);
    }

    /// Convenience CPU buffer helper: composites `shapes` over `backdrop_rgba` (`width * height * 4`)
    /// directly into caller-provided `out_rgba` (`width * height * 4`) using the GPU.
    pub fn render_rgba8(
        &mut self,
        backdrop_rgba: &[u8],
        width: u32,
        height: u32,
        shapes: &[LgShape],
        out_rgba: &mut [u8],
    ) -> Result<(), String> {
        let expected = (width as usize) * (height as usize) * 4;
        if backdrop_rgba.len() != expected || out_rgba.len() != expected {
            return Err(format!(
                "buffer size mismatch: expected {expected} bytes for {width}x{height} RGBA8"
            ));
        }

        let extent = wgpu::Extent3d { width, height, depth_or_array_layers: 1 };
        let backdrop_tex = self.device.create_texture(&wgpu::TextureDescriptor {
            label: Some("lg_cpu_backdrop"),
            size: extent,
            mip_level_count: 1,
            sample_count: 1,
            dimension: wgpu::TextureDimension::D2,
            format: wgpu::TextureFormat::Rgba8Unorm,
            usage: wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_DST | wgpu::TextureUsages::COPY_SRC,
            view_formats: &[],
        });
        self.queue.write_texture(
            backdrop_tex.as_image_copy(),
            backdrop_rgba,
            wgpu::TexelCopyBufferLayout {
                offset: 0,
                bytes_per_row: Some(width * 4),
                rows_per_image: Some(height),
            },
            extent,
        );

        let target_tex = self.device.create_texture(&wgpu::TextureDescriptor {
            label: Some("lg_cpu_target"),
            size: extent,
            mip_level_count: 1,
            sample_count: 1,
            dimension: wgpu::TextureDimension::D2,
            format: wgpu::TextureFormat::Rgba8Unorm,
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::COPY_DST | wgpu::TextureUsages::COPY_SRC,
            view_formats: &[],
        });

        // Seed target with backdrop so premultiplied glass composites over it identically to a compositor.
        let mut init_enc = self.device.create_command_encoder(&wgpu::CommandEncoderDescriptor::default());
        init_enc.copy_texture_to_texture(backdrop_tex.as_image_copy(), target_tex.as_image_copy(), extent);
        self.queue.submit(Some(init_enc.finish()));

        let backdrop_view = backdrop_tex.create_view(&wgpu::TextureViewDescriptor::default());
        let target_view = target_tex.create_view(&wgpu::TextureViewDescriptor::default());
        let bdesc = LgBackdropDesc {
            texture_id: 0,
            width,
            height,
            uv_rect: [0.0, 0.0, 1.0, 1.0],
            y_origin: LgYOrigin::TopLeft,
            format: LgPixelFormat::Rgba8Unorm,
        };
        let tdesc = LgTargetDesc {
            y_origin: LgYOrigin::TopLeft,
            format: LgPixelFormat::Rgba8Unorm,
            premultiplied_output: 1,
            ..LgTargetDesc::default()
        };
        self.render_borrowed(&backdrop_view, &bdesc, &target_view, &tdesc, shapes, &[]);

        // Readback into caller-provided `out_rgba`.
        let unpadded_bpr = width * 4;
        let align = wgpu::COPY_BYTES_PER_ROW_ALIGNMENT;
        let padded_bpr = unpadded_bpr.div_ceil(align) * align;
        let readback = self.device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("lg_cpu_readback"),
            size: (padded_bpr as u64) * (height as u64),
            usage: wgpu::BufferUsages::COPY_DST | wgpu::BufferUsages::MAP_READ,
            mapped_at_creation: false,
        });
        let mut enc = self.device.create_command_encoder(&wgpu::CommandEncoderDescriptor::default());
        enc.copy_texture_to_buffer(
            target_tex.as_image_copy(),
            wgpu::TexelCopyBufferInfo {
                buffer: &readback,
                layout: wgpu::TexelCopyBufferLayout {
                    offset: 0,
                    bytes_per_row: Some(padded_bpr),
                    rows_per_image: Some(height),
                },
            },
            extent,
        );
        self.queue.submit(Some(enc.finish()));

        let slice = readback.slice(..);
        let (tx, rx) = std::sync::mpsc::channel();
        slice.map_async(wgpu::MapMode::Read, move |r| {
            let _ = tx.send(r);
        });
        let _ = self.device.poll(wgpu::PollType::wait_indefinitely());
        rx.recv()
            .map_err(|e| format!("readback channel error: {e}"))?
            .map_err(|e| format!("readback map error: {e}"))?;

        let mapped = slice
            .get_mapped_range()
            .map_err(|e| format!("readback get_mapped_range error: {e}"))?;
        for y in 0..height as usize {
            let src_off = y * padded_bpr as usize;
            let dst_off = y * unpadded_bpr as usize;
            out_rgba[dst_off..dst_off + unpadded_bpr as usize]
                .copy_from_slice(&mapped[src_off..src_off + unpadded_bpr as usize]);
        }
        drop(mapped);
        readback.unmap();
        Ok(())
    }
}

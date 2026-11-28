// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Build-time shader codegen via `naga` (single WGSL source -> WGSL, SPIR-V, GLSL 330 core,
// GLSL ES 3.00, MSL, HLSL) + C header generation via `cbindgen` when `ffi` is enabled.

use std::env;
use std::fs;
use std::path::{Path, PathBuf};

use naga::back::{glsl, hlsl, msl, spv, wgsl};
use naga::front::wgsl as wgsl_in;
use naga::valid::{Capabilities, ValidationFlags, Validator};

fn compile_shader(name: &str, wgsl_src: &str, out_dir: &Path) {
    let module = wgsl_in::parse_str(wgsl_src)
        .unwrap_or_else(|e| panic!("WGSL parse failed for {name}: {}", e.emit_to_string(wgsl_src)));

    let info = Validator::new(ValidationFlags::all(), Capabilities::empty())
        .validate(&module)
        .unwrap_or_else(|e| panic!("naga validation failed for {name}: {e:?}"));

    // 1. Validated WGSL output
    let wgsl_out = wgsl::write_string(&module, &info, wgsl::WriterFlags::empty())
        .unwrap_or_else(|e| panic!("WGSL emit failed for {name}: {e:?}"));
    fs::write(out_dir.join(format!("{name}.wgsl")), wgsl_out).unwrap();

    // 2. SPIR-V output (Vulkan / OpenGL SPIR-V)
    let spv_opts = spv::Options {
        lang_version: (1, 3),
        flags: spv::WriterFlags::ADJUST_COORDINATE_SPACE,
        ..Default::default()
    };
    let spv_words = spv::write_vec(&module, &info, &spv_opts, None)
        .unwrap_or_else(|e| panic!("SPIR-V emit failed for {name}: {e:?}"));
    let spv_bytes: Vec<u8> = spv_words.iter().flat_map(|w| w.to_le_bytes()).collect();
    fs::write(out_dir.join(format!("{name}.spv")), spv_bytes).unwrap();

    // 3. GLSL 330 core (Desktop OpenGL) & GLSL ES 3.00 (GLES / Wayland / Android / WebGL2)
    for (suffix, version) in [
        ("glsl330", glsl::Version::Desktop(330)),
        ("gles300", glsl::Version::Embedded { version: 300, is_webgl: false }),
    ] {
        for (stage_suffix, stage, entry) in [
            ("vert", naga::ShaderStage::Vertex, "vs_main"),
            ("frag", naga::ShaderStage::Fragment, "fs_main"),
        ] {
            let mut glsl_buf = String::new();
            let opts = glsl::Options {
                version,
                writer_flags: glsl::WriterFlags::empty(),
                ..Default::default()
            };
            let pipeline_opts = glsl::PipelineOptions {
                shader_stage: stage,
                entry_point: entry.to_string(),
                multiview: None,
            };
            let mut writer = glsl::Writer::new(
                &mut glsl_buf,
                &module,
                &info,
                &opts,
                &pipeline_opts,
                naga::proc::BoundsCheckPolicies::default(),
            )
            .unwrap_or_else(|e| panic!("GLSL ({suffix}/{stage_suffix}) writer init failed for {name}: {e:?}"));
            writer
                .write()
                .unwrap_or_else(|e| panic!("GLSL ({suffix}/{stage_suffix}) emit failed for {name}: {e:?}"));
            fs::write(out_dir.join(format!("{name}.{suffix}.{stage_suffix}")), glsl_buf).unwrap();
        }
    }

    // 4. MSL output (Apple Metal)
    let msl_opts = msl::Options {
        lang_version: (2, 1),
        ..Default::default()
    };
    let (msl_src, _) = msl::write_string(&module, &info, &msl_opts, &msl::PipelineOptions::default())
        .unwrap_or_else(|e| panic!("MSL emit failed for {name}: {e:?}"));
    fs::write(out_dir.join(format!("{name}.metal")), msl_src).unwrap();

    // 5. HLSL output (DirectX 12)
    let mut hlsl_buf = String::new();
    let hlsl_opts = hlsl::Options {
        shader_model: hlsl::ShaderModel::V5_1,
        ..Default::default()
    };
    let hlsl_pipeline_opts = hlsl::PipelineOptions::default();
    let mut hlsl_writer = hlsl::Writer::new(&mut hlsl_buf, &hlsl_opts, &hlsl_pipeline_opts);
    hlsl_writer
        .write(&module, &info, None)
        .unwrap_or_else(|e| panic!("HLSL emit failed for {name}: {e:?}"));
    fs::write(out_dir.join(format!("{name}.hlsl")), hlsl_buf).unwrap();
}

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());
    let shaders_dir = manifest_dir.join("shaders");

    for name in ["kawase_down", "kawase_up", "liquidglass"] {
        let path = shaders_dir.join(format!("{name}.wgsl"));
        println!("cargo:rerun-if-changed={}", path.display());
        let src = fs::read_to_string(&path).unwrap();
        compile_shader(name, &src, &out_dir);
    }

    // Generate C header when `ffi` feature is enabled.
    println!("cargo:rerun-if-changed=cbindgen.toml");
    println!("cargo:rerun-if-changed=src/core.rs");
    println!("cargo:rerun-if-changed=src/ffi.rs");
    if env::var_os("CARGO_FEATURE_FFI").is_some() {
        let include_dir = manifest_dir.join("include");
        fs::create_dir_all(&include_dir).unwrap();
        let config = cbindgen::Config::from_file(manifest_dir.join("cbindgen.toml")).unwrap();
        cbindgen::Builder::new()
            .with_crate(&manifest_dir)
            .with_config(config)
            .generate()
            .expect("cbindgen failed to generate include/liquidglass.h")
            .write_to_file(include_dir.join("liquidglass.h"));
    }
}

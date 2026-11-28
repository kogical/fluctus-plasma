# liquidglass

The Rust engine that renders Liquid Glass for [Fluctus Glass](../../glass/README.md). It knows
nothing about KWin or Qt: hand it a backdrop texture, a target and a description of the glass,
and it draws.

- **One shader source.** The look is written once in WGSL (`shaders/`). At build time `naga`
  turns it into GLSL, GLSL ES, SPIR-V, MSL and HLSL.
- **Zero-copy OpenGL backend** (`gl` feature). It borrows the host's GL context, textures and
  framebuffer, which is how the KWin effect uses it.
- **wgpu backend** (`wgpu` feature). Cross-platform rendering, used by the headless GPU tests.
- **C ABI** (`ffi` feature). A stable, allocation-free API with a generated header in
  [include/liquidglass.h](include/liquidglass.h).
- **Core** (always on). SDF shapes, bezel and Kawase math, pointer physics and uniform packing,
  with no GPU dependency at all.

## Working on it

```bash
just liquidglass test       # core + shader codegen tests
just liquidglass test-gpu   # headless wgpu rendering tests (needs a GPU driver)
just liquidglass check      # the gl + ffi build the KWin effect links
just liquidglass header     # regenerate include/liquidglass.h
just liquidglass doc        # API docs
```

The crate belongs to the Cargo workspace at the repository root, so `cargo` works from
anywhere in the tree. Every build, including the one CMake runs for the effect, shares the
root `target/` directory.

The parity test compares renders with Apple's reference images. Those images cannot be
redistributed, so it looks for them in `$FLUCTUS_PARITY_REFS` (default
`~/.cache/fluctus/parity-refs`) and skips itself when they are missing.

Licensed under [Apache-2.0](LICENSE).

// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Flat, allocation-free C ABI for plugging `liquidglass` into KWin C++, Flutter Dart FFI, Qt,
// GTK, Swift, or any C-compatible host. `cbindgen` emits `include/liquidglass.h` from this file
// and `src/core.rs`.

use core::ffi::{c_char, c_void};

use crate::core::{
    expand_for_iterations, kawase_for_strength, strength_for_frost, LgMaterial, LgRenderParams,
    LgStatus,
};

/// Opaque handle to a zero-copy OpenGL / OpenGL ES Liquid Glass renderer.
#[repr(C)]
pub struct LgGlRenderer {
    _private: [u8; 0],
}

/// Host callback resolving OpenGL function pointers by null-terminated symbol name
/// (e.g. `eglGetProcAddress` or `glXGetProcAddress`).
pub type LgGlProcAddr = Option<unsafe extern "C" fn(name: *const c_char, user_data: *mut c_void) -> *const c_void>;

/// Returns the default Apple-accurate Liquid Glass material parameters.
#[unsafe(no_mangle)]
pub extern "C" fn lg_material_default() -> LgMaterial {
    LgMaterial::default()
}

/// Returns the ultra-clear Crystal preset material parameters.
#[unsafe(no_mangle)]
pub extern "C" fn lg_material_crystal() -> LgMaterial {
    LgMaterial::crystal()
}

/// Returns the logical pixel sampling margin required around a shape for refraction & dispersion.
#[unsafe(no_mangle)]
pub extern "C" fn lg_material_sample_margin(material: *const LgMaterial) -> f32 {
    if material.is_null() {
        return 0.0;
    }
    unsafe { (*material).sample_margin() }
}

/// Returns the logical pixel draw margin required around a shape for shadow & outer borders.
#[unsafe(no_mangle)]
pub extern "C" fn lg_material_draw_margin(material: *const LgMaterial) -> f32 {
    if material.is_null() {
        return 0.0;
    }
    unsafe { (*material).draw_margin() }
}

/// Returns the backdrop capture expansion (in device pixels) needed by Dual Kawase for `frost` at `scale`.
#[unsafe(no_mangle)]
pub extern "C" fn lg_kawase_expand_for_frost(frost: f32, scale: f32) -> u32 {
    let step = kawase_for_strength(strength_for_frost(frost), scale);
    expand_for_iterations(step.iterations)
}

/// Creates a zero-copy OpenGL / GLES Liquid Glass renderer on the currently active GL context.
///
/// # Safety
/// An OpenGL 3.3+ or OpenGL ES 3.0+ context must be current on the calling thread, `get_proc_address`
/// must be non-null and return valid GL entry points, and `out_renderer` must be a valid pointer.
#[cfg(feature = "gl")]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lg_gl_create(
    get_proc_address: LgGlProcAddr,
    user_data: *mut c_void,
    out_renderer: *mut *mut LgGlRenderer,
) -> LgStatus {
    use std::ffi::CString;

    if out_renderer.is_null() {
        return LgStatus::NullPointer;
    }
    unsafe { *out_renderer = core::ptr::null_mut() };
    let Some(loader_fn) = get_proc_address else {
        return LgStatus::NullPointer;
    };

    let res = unsafe {
        crate::gl_backend::GlRenderer::new(|symbol: &str| {
            let Ok(cstr) = CString::new(symbol) else {
                return core::ptr::null();
            };
            loader_fn(cstr.as_ptr(), user_data)
        })
    };

    match res {
        Ok(renderer) => {
            let boxed = Box::new(renderer);
            unsafe { *out_renderer = Box::into_raw(boxed).cast::<LgGlRenderer>() };
            LgStatus::Ok
        }
        Err(status) => status,
    }
}

/// Renders one frame of Liquid Glass shapes from a borrowed backdrop texture into a borrowed target FBO.
/// Allocation-free in steady state; strictly saves and restores all touched GL state.
///
/// # Safety
/// `renderer` must be a live handle from `lg_gl_create`, the same GL context must be current, and
/// `params` (and its inner slices) must be valid for the call duration.
#[cfg(feature = "gl")]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lg_gl_render(
    renderer: *mut LgGlRenderer,
    params: *const LgRenderParams,
) -> LgStatus {
    if renderer.is_null() || params.is_null() {
        return LgStatus::NullPointer;
    }
    let r = unsafe { &mut *renderer.cast::<crate::gl_backend::GlRenderer>() };
    let p = unsafe { &*params };
    unsafe { r.render(p) }
}

/// Releases pooled internal scratch blur textures/FBOs without destroying compiled shaders.
///
/// # Safety
/// `renderer` must be null or a live handle from `lg_gl_create`, and the GL context must be current.
#[cfg(feature = "gl")]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lg_gl_invalidate_caches(renderer: *mut LgGlRenderer) {
    if renderer.is_null() {
        return;
    }
    let r = unsafe { &mut *renderer.cast::<crate::gl_backend::GlRenderer>() };
    unsafe { r.invalidate_caches() };
}

/// Destroys all owned GL resources and frees the renderer handle.
///
/// # Safety
/// `renderer` must be null or a live handle from `lg_gl_create` (never freed twice), and the GL
/// context must be current so GL objects are cleanly deleted.
#[cfg(feature = "gl")]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lg_gl_destroy(renderer: *mut LgGlRenderer) {
    if renderer.is_null() {
        return;
    }
    let boxed = unsafe { Box::from_raw(renderer.cast::<crate::gl_backend::GlRenderer>()) };
    unsafe { boxed.destroy() };
}

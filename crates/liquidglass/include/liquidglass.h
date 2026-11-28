#ifndef LIQUIDGLASS_H
#define LIQUIDGLASS_H

#pragma once

/* Generated automatically by cbindgen from crates/liquidglass — do not edit by hand. */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * Result code returned by C FFI entry points.
 */
typedef enum LgStatus {
  LG_STATUS_OK = 0,
  LG_STATUS_NULL_POINTER = 1,
  LG_STATUS_INVALID_ARGUMENT = 2,
  LG_STATUS_GL_COMPILE_ERROR = 3,
  LG_STATUS_GL_FRAMEBUFFER_ERROR = 4,
  LG_STATUS_UNSUPPORTED = 5,
} LgStatus;

/**
 * Vertical texture coordinate origin of the host graphics API.
 */
typedef enum LgYOrigin {
  /**
   * Vulkan, Metal, DX12, WebGPU, Wayland SHM, CPU pixel buffers (`v = 0` at top).
   */
  LG_Y_ORIGIN_TOP_LEFT = 0,
  /**
   * OpenGL / OpenGL ES textures & FBOs (`v = 0` at bottom).
   */
  LG_Y_ORIGIN_BOTTOM_LEFT = 1,
} LgYOrigin;

/**
 * Pixel format of host backdrop / target textures (supports SDR `RGBA8` and KWin HDR `RGBA16F`).
 */
typedef enum LgPixelFormat {
  LG_PIXEL_FORMAT_RGBA8_UNORM = 0,
  LG_PIXEL_FORMAT_BGRA8_UNORM = 1,
  LG_PIXEL_FORMAT_RGBA16_FLOAT = 2,
  LG_PIXEL_FORMAT_RGBA8_UNORM_SRGB = 3,
} LgPixelFormat;

/**
 * Straight-alpha RGBA color in the output color space (SDR `0..1`, or HDR linear/mapped).
 */
typedef struct LgColor {
  float r;
  float g;
  float b;
  float a;
} LgColor;
#define LgColor_TRANSPARENT (LgColor){ .r = 0.0, .g = 0.0, .b = 0.0, .a = 0.0 }

/**
 * Physical Liquid Glass material parameters (all spatial lengths in logical pixels unless noted).
 */
typedef struct LgMaterial {
  /**
   * Master effect strength `0..2` scaling refraction, magnification, dispersion, bulge, and rim.
   */
  float intensity;
  /**
   * Straight-alpha tint color blended over the graded refracted backdrop.
   */
  struct LgColor tint;
  /**
   * Curved bezel width in logical px.
   */
  float bezel;
  /**
   * Max bezel refraction displacement in logical px.
   */
  float refraction;
  /**
   * Smooth pillow-dome magnification `0..0.45`.
   */
  float magnify;
  /**
   * Extra outer-lip refraction fold `0..1`.
   */
  float edge_bend;
  /**
   * Spectral Cauchy chromatic dispersion factor `0..1`.
   */
  float dispersion;
  /**
   * Interior backdrop diffusion strength `0..1` (0 = sharp, ~0.24 = Liquid Glass, 0.75+ = frosted).
   */
  float frost;
  /**
   * Backdrop saturation multiplier.
   */
  float saturation;
  /**
   * Backdrop brightness multiplier.
   */
  float brightness;
  /**
   * Optional mid-bezel refractive darkening band `0..1`.
   */
  float edge_shade;
  /**
   * Film grain `0..1`.
   */
  float grain;
  /**
   * Dual-corner specular rim intensity (`CASDFGlassHighlightEffect`).
   */
  float highlight;
  /**
   * Specular rim Gaussian half-width in logical px.
   */
  float highlight_width;
  /**
   * Optional directional surface sheen `0..1`.
   */
  float sheen;
  /**
   * Key light angle in degrees (y down; `-135.0` = top-left key + bottom-right opposite bounce).
   */
  float light_angle_deg;
  /**
   * Optional inner hairline border (straight-alpha RGBA).
   */
  struct LgColor inner_border;
  /**
   * Optional outer hairline border (straight-alpha RGBA).
   */
  struct LgColor outer_border;
  /**
   * Border hairline width in logical px.
   */
  float border_width;
  /**
   * Outside-only soft elevation drop shadow color (straight-alpha RGBA).
   */
  struct LgColor shadow_color;
  /**
   * Drop shadow softness in logical px.
   */
  float shadow_blur;
  /**
   * Drop shadow X offset in logical px.
   */
  float shadow_offset_x;
  /**
   * Drop shadow Y offset in logical px.
   */
  float shadow_offset_y;
  /**
   * Pointer-driven rim swing + light spot strength `0..1`.
   */
  float pointer_light;
  /**
   * Pointer-driven water-drop bulge displacement in logical px.
   */
  float pointer_bulge;
  /**
   * Pointer bulge Gaussian radius in logical px.
   */
  float pointer_radius;
} LgMaterial;

/**
 * Host callback resolving OpenGL function pointers by null-terminated symbol name
 * (e.g. `eglGetProcAddress` or `glXGetProcAddress`).
 */
typedef const void *(*LgGlProcAddr)(const char *name, void *user_data);

/**
 * Opaque handle to a zero-copy OpenGL / OpenGL ES Liquid Glass renderer.
 */
typedef struct LgGlRenderer {
  uint8_t _private[0];
} LgGlRenderer;

/**
 * Borrowed host backdrop texture descriptor (zero-copy).
 */
typedef struct LgBackdropDesc {
  /**
   * Borrowed GL texture name (`GLuint`) when using the `gl` backend; ignored by `wgpu`.
   */
  uint32_t texture_id;
  /**
   * Full texture width in device pixels.
   */
  uint32_t width;
  /**
   * Full texture height in device pixels.
   */
  uint32_t height;
  /**
   * Normalized sub-rect `(u0, v0, u1, v1)` within the backdrop texture (default `0, 0, 1, 1`).
   */
  float uv_rect[4];
  /**
   * Vertical origin of the backdrop texture (`BottomLeft` for GL, `TopLeft` for Vulkan/Metal/CPU).
   */
  enum LgYOrigin y_origin;
  /**
   * Pixel format of the backdrop texture (used to allocate matching internal scratch blur targets).
   */
  enum LgPixelFormat format;
} LgBackdropDesc;

/**
 * Integer rectangle (e.g. GL viewport / scissor box).
 */
typedef struct LgRectI {
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
} LgRectI;

/**
 * Floating-point rectangle in capture-local device pixels (y pointing down).
 */
typedef struct LgRect {
  float x;
  float y;
  float width;
  float height;
} LgRect;

/**
 * Borrowed host render target descriptor (zero-copy).
 */
typedef struct LgTargetDesc {
  /**
   * Borrowed GL framebuffer handle (`GLuint`; `0` = default framebuffer).
   */
  uint32_t fbo_id;
  /**
   * Target viewport rectangle `(x, y, width, height)` in GL viewport coordinates.
   * If `width == 0 || height == 0`, the currently bound GL viewport is preserved.
   */
  struct LgRectI viewport;
  /**
   * Column-major 4x4 clip-space projection matrix mapping capture-local device px to NDC.
   * If `use_custom_projection == 0`, an orthographic matrix is built from `backdrop` size & `y_origin`.
   */
  float projection[16];
  /**
   * Non-zero to use `projection`; `0` to auto-generate orthographic projection.
   */
  uint8_t use_custom_projection;
  /**
   * Non-zero if output should be premultiplied alpha (`GL_ONE, GL_ONE_MINUS_SRC_ALPHA`).
   */
  uint8_t premultiplied_output;
  /**
   * Vertical origin of the render target.
   */
  enum LgYOrigin y_origin;
  /**
   * Pixel format of the render target.
   */
  enum LgPixelFormat format;
  /**
   * Reference white in target color space (`[1.0, 1.0, 1.0]` for SDR, mapped white for KWin HDR).
   */
  float reference_white[3];
  /**
   * Optional damage clip rectangles in capture-local device px (null / 0 = full draw extent).
   */
  const struct LgRect *clip_rects;
  uint32_t clip_rect_count;
} LgTargetDesc;

/**
 * Interactive pointer state for a glass shape (in capture-local or shape-local device px).
 */
typedef struct LgPointerState {
  /**
   * Pointer X relative to the shape's top-left corner (in device px).
   */
  float x;
  /**
   * Pointer Y relative to the shape's top-left corner (in device px).
   */
  float y;
  /**
   * Smoothly animated presence `0..1`.
   */
  float strength;
} LgPointerState;

/**
 * One rounded-rect glass shape to render in capture-local device pixels.
 */
typedef struct LgShape {
  /**
   * Glass rect `(x, y, width, height)` in capture-local device pixels (y down).
   */
  struct LgRect rect;
  /**
   * Corner radius in device pixels.
   */
  float radius;
  /**
   * Output opacity `0..1`.
   */
  float opacity;
  /**
   * Device pixel scale factor (`scale = device_px / logical_px`, e.g. `1.0`, `1.5`, `2.0`).
   */
  float scale;
  /**
   * Material parameters (logical px; scaled by `scale` internally when packing uniforms).
   */
  struct LgMaterial material;
  /**
   * Interactive pointer state (shape-local device px).
   */
  struct LgPointerState pointer;
} LgShape;

/**
 * Top-level render batch descriptor passed across the FFI boundary (zero allocations).
 */
typedef struct LgRenderParams {
  struct LgBackdropDesc backdrop;
  struct LgTargetDesc target;
  const struct LgShape *shapes;
  uint32_t shape_count;
} LgRenderParams;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Returns the default Apple-accurate Liquid Glass material parameters.
 */
struct LgMaterial lg_material_default(void);

/**
 * Returns the ultra-clear Crystal preset material parameters.
 */
struct LgMaterial lg_material_crystal(void);

/**
 * Returns the logical pixel sampling margin required around a shape for refraction & dispersion.
 *
 * # Safety
 * `material` must be null or point to a valid, initialized `LgMaterial`.
 */
float lg_material_sample_margin(const struct LgMaterial *material);

/**
 * Returns the logical pixel draw margin required around a shape for shadow & outer borders.
 *
 * # Safety
 * `material` must be null or point to a valid, initialized `LgMaterial`.
 */
float lg_material_draw_margin(const struct LgMaterial *material);

/**
 * Returns the backdrop capture expansion (in device pixels) needed by Dual Kawase for `frost` at `scale`.
 */
uint32_t lg_kawase_expand_for_frost(float frost,
                                    float scale);

/**
 * Creates a zero-copy OpenGL / GLES Liquid Glass renderer on the currently active GL context.
 *
 * # Safety
 * An OpenGL 3.3+ or OpenGL ES 3.0+ context must be current on the calling thread, `get_proc_address`
 * must be non-null and return valid GL entry points, and `out_renderer` must be a valid pointer.
 */
enum LgStatus lg_gl_create(LgGlProcAddr get_proc_address,
                           void *user_data,
                           struct LgGlRenderer **out_renderer);

/**
 * Renders one frame of Liquid Glass shapes from a borrowed backdrop texture into a borrowed target FBO.
 * Allocation-free in steady state; strictly saves and restores all touched GL state.
 *
 * # Safety
 * `renderer` must be a live handle from `lg_gl_create`, the same GL context must be current, and
 * `params` (and its inner slices) must be valid for the call duration.
 */
enum LgStatus lg_gl_render(struct LgGlRenderer *renderer,
                           const struct LgRenderParams *params);

/**
 * Releases pooled internal scratch blur textures/FBOs without destroying compiled shaders.
 *
 * # Safety
 * `renderer` must be null or a live handle from `lg_gl_create`, and the GL context must be current.
 */
void lg_gl_invalidate_caches(struct LgGlRenderer *renderer);

/**
 * Destroys all owned GL resources and frees the renderer handle.
 *
 * # Safety
 * `renderer` must be null or a live handle from `lg_gl_create` (never freed twice), and the GL
 * context must be current so GL objects are cleanly deleted.
 */
void lg_gl_destroy(struct LgGlRenderer *renderer);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  /* LIQUIDGLASS_H */

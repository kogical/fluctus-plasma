// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Apple-accurate Liquid Glass shader (single source of truth in WGSL; naga emits WGSL, SPIR-V,
// GLSL 330 core, GLSL ES 3.00, MSL, and HLSL at build time).
//
// Optical pipeline (verified against macOS 26 Tahoe QuartzCore & inverse-optics measurements):
//   1. Continuous superellipse SDF (n = 2.2 on rounded rects, relaxing to n = 2.0 on circles/capsules)
//   2. Smooth convex pillow-dome magnification + C1-smooth two-zone opposite-sign bezel refraction
//      (negative inward Snell inner zone + positive outward outer-lip fold)
//   3. 5-tap spectral Cauchy dispersion (n_B > n_G > n_R) coupled strictly to the curved bezel
//   4. Progressive bezel clarity (diffused interior -> crystal-sharp outer rim)
//   5. Dual opposing-corner specular rim (-135 deg / +45 deg) modulated by Apple's verified
//      QuartzCore vibrantColorMatrix + luminance-adaptive gain
//   6. Outside-only soft elevation shadow + optional inner/outer borders + film grain + pointer lighting

struct GlassUniforms {
    projection: mat4x4<f32>,
    glass_rect: vec4<f32>,     // (x, y, w, h) in capture-local device px (y down)
    uv_rect: vec4<f32>,        // sub-rect (u0, v0, u1, v1) in backdrop texture
    uv_clamp: vec4<f32>,       // (u_min, v_min, u_max, v_max)
    tint: vec4<f32>,           // straight-alpha RGBA (already color-space mapped)
    inner_border: vec4<f32>,   // straight-alpha RGBA
    outer_border: vec4<f32>,   // straight-alpha RGBA
    shadow_color: vec4<f32>,   // straight-alpha RGBA
    tex_and_blur: vec4<f32>,   // (tex_w, tex_h, blur_half_x, blur_half_y)
    optics_a: vec4<f32>,       // (radius, bezel, refraction, dispersion)
    optics_b: vec4<f32>,       // (magnify, edge_bend, saturation, brightness)
    rim_params: vec4<f32>,     // (rim_light, rim_width, light_dir_x, light_dir_y)
    surface_a: vec4<f32>,      // (edge_shade, sheen, border_width, grain)
    shadow_a: vec4<f32>,       // (shadow_blur, shadow_off_x, shadow_off_y, px)
    pointer_a: vec4<f32>,      // (pointer_x, pointer_y, pointer_strength, pointer_light)
    pointer_b: vec4<f32>,      // (pointer_bulge, pointer_radius, opacity, use_blur)
    misc: vec4<f32>,           // (blur_offset, y_bottom_up, premult_out, _pad)
    white_ref: vec4<f32>,      // (white.r, white.g, white.b, _pad)
};

@group(0) @binding(0) var<uniform> u: GlassUniforms;
@group(0) @binding(1) var sharp_tex: texture_2d<f32>;
@group(0) @binding(2) var sharp_samp: sampler;
@group(0) @binding(3) var blur_tex: texture_2d<f32>;
@group(0) @binding(4) var blur_samp: sampler;

struct VsIn {
    @location(0) pos: vec2<f32>,
};

struct VsOut {
    @builtin(position) clip_pos: vec4<f32>,
    @location(0) local_pos: vec2<f32>,
};

@vertex
fn vs_main(in: VsIn) -> VsOut {
    var out: VsOut;
    out.clip_pos = u.projection * vec4<f32>(in.pos, 0.0, 1.0);
    out.local_pos = in.pos;
    return out;
}

struct Geom {
    lp: vec2<f32>,
    half_size: vec2<f32>,
    min_half: f32,
    r: f32,
    d: f32,
    inside: f32,
    depth: f32,
    n: vec2<f32>,
    eff_bezel: f32,
    t: f32,
    lens: f32,
    outer_lens: f32,
};

fn sd_round_rect(p: vec2<f32>, half_size: vec2<f32>, r: f32) -> f32 {
    let q = abs(p) - half_size + vec2<f32>(r, r);
    if (r > 0.5 && q.x > 0.0 && q.y > 0.0) {
        let min_h = max(min(half_size.x, half_size.y), 1.0);
        let circle_mix = smoothstep(0.76, 0.98, r / min_h);
        let n = mix(2.2, 2.0, circle_mix);
        let uq = q / r;
        let s = pow(pow(uq.x, n) + pow(uq.y, n), 1.0 / n);
        return (s - 1.0) * r;
    }
    return length(max(q, vec2<f32>(0.0, 0.0))) + min(max(q.x, q.y), 0.0) - r;
}

fn sd_normal(p: vec2<f32>, half_size: vec2<f32>, r: f32) -> vec2<f32> {
    let e = 0.5;
    let g = vec2<f32>(
        sd_round_rect(p + vec2<f32>(e, 0.0), half_size, r) - sd_round_rect(p - vec2<f32>(e, 0.0), half_size, r),
        sd_round_rect(p + vec2<f32>(0.0, e), half_size, r) - sd_round_rect(p - vec2<f32>(0.0, e), half_size, r),
    );
    let l = length(g);
    if (l > 1e-5) {
        return g / l;
    }
    return vec2<f32>(0.0, -1.0);
}

fn snell_bezel(x: f32) -> f32 {
    let xc = clamp(x, 0.0, 1.0);
    let a = 1.0 - xc;
    let b = max(1.0 - a * a, 1e-4);
    let height = sqrt(b);
    let smooth_tail = 1.0 - xc * xc * (3.0 - 2.0 * xc);
    let slope = min(a / height, 4.5) * smooth_tail;
    let n3 = normalize(vec3<f32>(-slope, 0.0, 1.0));
    let t3 = refract(vec3<f32>(0.0, 0.0, -1.0), n3, 1.0 / 1.5);
    let lateral = abs(t3.x) / max(-t3.z, 1e-4);
    let slab_h = 0.70;
    return min(lateral * (slab_h + height) / 0.72, 1.0);
}

fn hash12(p: vec2<f32>) -> f32 {
    var p3 = fract(vec3<f32>(p.x, p.y, p.x) * 0.1031);
    p3 += vec3<f32>(dot(p3, vec3<f32>(p3.y, p3.z, p3.x) + vec3<f32>(33.33)));
    return fract((p3.x + p3.y) * p3.z);
}

fn over(top: vec4<f32>, bottom: vec4<f32>) -> vec4<f32> {
    return top + bottom * (1.0 - top.a);
}

fn to_uv(p: vec2<f32>) -> vec2<f32> {
    let nx = p.x / max(u.tex_and_blur.x, 1.0);
    var ny = p.y / max(u.tex_and_blur.y, 1.0);
    if (u.misc.y > 0.5) {
        ny = 1.0 - ny;
    }
    let uv = vec2<f32>(
        mix(u.uv_rect.x, u.uv_rect.z, nx),
        mix(u.uv_rect.y, u.uv_rect.w, ny),
    );
    return clamp(uv, u.uv_clamp.xy, u.uv_clamp.zw);
}

fn sample_backdrop_blur(p: vec2<f32>) -> vec4<f32> {
    let uv = to_uv(p);
    if (u.pointer_b.w < 0.5) {
        return textureSample(sharp_tex, sharp_samp, uv);
    }
    let h = u.tex_and_blur.zw * u.misc.x;
    var sum = textureSample(blur_tex, blur_samp, uv + vec2<f32>(-h.x * 2.0, 0.0));
    sum += textureSample(blur_tex, blur_samp, uv + vec2<f32>(-h.x, h.y)) * 2.0;
    sum += textureSample(blur_tex, blur_samp, uv + vec2<f32>(0.0, h.y * 2.0));
    sum += textureSample(blur_tex, blur_samp, uv + vec2<f32>(h.x, h.y)) * 2.0;
    sum += textureSample(blur_tex, blur_samp, uv + vec2<f32>(h.x * 2.0, 0.0));
    sum += textureSample(blur_tex, blur_samp, uv + vec2<f32>(h.x, -h.y)) * 2.0;
    sum += textureSample(blur_tex, blur_samp, uv + vec2<f32>(0.0, -h.y * 2.0));
    sum += textureSample(blur_tex, blur_samp, uv + vec2<f32>(-h.x, -h.y)) * 2.0;
    return sum / 12.0;
}

fn sample_backdrop(p: vec2<f32>, clarity: f32) -> vec4<f32> {
    let blurred = sample_backdrop_blur(p);
    if (u.pointer_b.w < 0.5 || clarity <= 0.001) {
        return blurred;
    }
    let sharp = textureSample(sharp_tex, sharp_samp, to_uv(p));
    return mix(blurred, sharp, clarity);
}

fn compute_geometry(p: vec2<f32>) -> Geom {
    var g: Geom;
    let px = max(u.shadow_a.w, 1e-4);
    g.half_size = u.glass_rect.zw * 0.5;
    g.min_half = max(min(g.half_size.x, g.half_size.y), 1.0);
    let centre = u.glass_rect.xy + g.half_size;
    g.r = clamp(u.optics_a.x, 0.0, g.min_half);
    g.lp = p - centre;
    g.d = sd_round_rect(g.lp, g.half_size, g.r);
    g.inside = clamp(0.5 - g.d / px, 0.0, 1.0);
    g.depth = max(-g.d, 0.0);
    g.n = sd_normal(g.lp, g.half_size, g.r);
    g.eff_bezel = min(u.optics_a.y, g.min_half * 0.48);
    g.t = clamp(g.depth / max(g.eff_bezel, 0.001), 0.0, 1.0);
    g.lens = snell_bezel(g.t);
    let outer_w = max(g.eff_bezel * 0.36, 3.0 * px);
    let u_out = clamp(g.depth / outer_w, 0.0, 1.0);
    g.outer_lens = pow(1.0 - u_out, 2.2);
    return g;
}

fn dome_offset(g: Geom) -> vec2<f32> {
    let mag = u.optics_b.x;
    if (mag <= 0.0001) {
        return vec2<f32>(0.0, 0.0);
    }
    let dome_wall = pow(clamp(1.0 - g.depth / max(g.min_half * 0.95, 1.0), 0.0, 1.0), 1.6);
    let dome_m = mag * (0.78 + 0.52 * dome_wall);
    let lp_len = length(g.lp);
    let arm_scale = min(1.0, (g.min_half * 1.65) / max(lp_len, 1.0));
    return -g.lp * (dome_m * arm_scale);
}

fn lens_offset(g: Geom, p: vec2<f32>) -> vec2<f32> {
    let bezel = u.optics_a.y;
    var bezel_scale = 0.0;
    if (bezel > 0.001) {
        bezel_scale = clamp(g.eff_bezel / bezel, 0.0, 1.0);
    }
    let eff_ref = u.optics_a.z * bezel_scale;
    let edge_bend = u.optics_b.y;
    let dome = dome_offset(g);
    var o = -g.n * (eff_ref * g.lens)
        + (-dome + g.n * (eff_ref * (1.65 + 0.70 * edge_bend))) * g.outer_lens;
    let ptr = clamp(u.pointer_a.z, 0.0, 1.0);
    let ptr_bulge = u.pointer_b.x;
    if (ptr > 0.0 && ptr_bulge > 0.0) {
        let dp = (p - u.glass_rect.xy) - u.pointer_a.xy;
        let sigma = max(u.pointer_b.y, 1.0);
        let f = exp(-dot(dp, dp) / (2.0 * sigma * sigma));
        o -= dp * (ptr_bulge / sigma) * f * ptr;
    }
    return o;
}

fn grade_bg(bg: vec3<f32>) -> vec3<f32> {
    let w = u.white_ref.xyz;
    let lum = dot(bg, vec3<f32>(0.2126, 0.7152, 0.0722));
    let graded = max(mix(vec3<f32>(lum), bg, u.optics_b.z) * u.optics_b.w, vec3<f32>(0.0));
    let peak = max(max(bg.r, max(bg.g, bg.b)), max(w.r, max(w.g, w.b)));
    return min(graded, vec3<f32>(peak));
}

fn vibrant_highlight_color(rgb: vec3<f32>, white: vec3<f32>) -> vec3<f32> {
    let safe_w = max(white, vec3<f32>(1e-4));
    let c = clamp(rgb / safe_w, vec3<f32>(0.0), vec3<f32>(1.0));
    let vib = clamp(
        vec3<f32>(
             2.6705 * c.r - 1.1088 * c.g - 0.1117 * c.b + 0.05,
            -0.3295 * c.r + 1.8914 * c.g - 0.1119 * c.b + 0.05,
            -0.3297 * c.r - 1.1084 * c.g + 2.8881 * c.b + 0.05,
        ),
        vec3<f32>(0.0),
        vec3<f32>(1.0),
    );
    return (0.50 * vib + vec3<f32>(0.50)) * white;
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4<f32> {
    let p = in.local_pos;
    let g = compute_geometry(p);
    let w = u.white_ref.xyz;
    let px = max(u.shadow_a.w, 1e-4);

    var glass = vec4<f32>(0.0);
    if (g.inside > 0.0) {
        let lens_o = lens_offset(g, p);
        let o = dome_offset(g) + lens_o;
        let d = lens_o * u.optics_a.w;
        let clarity = clamp(pow(max(g.lens, g.outer_lens), 1.15) * 0.95, 0.0, 0.95);

        var rgb: vec3<f32>;
        if (dot(d, d) > 0.01) {
            let c_r  = sample_backdrop(p + o - d,       clarity).rgb;
            let c_rg = sample_backdrop(p + o - d * 0.5, clarity).rgb;
            let c_g  = sample_backdrop(p + o,           clarity).rgb;
            let c_gb = sample_backdrop(p + o + d * 0.5, clarity).rgb;
            let c_b  = sample_backdrop(p + o + d,       clarity).rgb;
            rgb = vec3<f32>(
                c_r.r * 0.65 + c_rg.r * 0.35,
                c_rg.g * 0.20 + c_g.g * 0.60 + c_gb.g * 0.20,
                c_gb.b * 0.35 + c_b.b * 0.65,
            );
        } else {
            rgb = sample_backdrop(p + o, clarity).rgb;
        }
        let bg = grade_bg(rgb);
        glass = vec4<f32>(mix(bg, u.tint.rgb, u.tint.a), 1.0);
    }

    // Optional mid-bezel refractive darkening band.
    let edge_shade = u.surface_a.x;
    if (edge_shade > 0.0001) {
        let band = sin(3.14159265 * g.t) * (1.0 - g.t * 0.35);
        let shade = clamp(edge_shade * band, 0.0, 1.0);
        glass = over(vec4<f32>(0.0, 0.0, 0.0, shade), glass);
    }

    // Key light (-135 deg top-left) swung interactively along the long axis towards the pointer.
    let gpx = p - u.glass_rect.xy;
    let dp = gpx - u.pointer_a.xy;
    let ptr = clamp(u.pointer_a.z, 0.0, 1.0);
    let l0 = normalize(u.rim_params.zw);
    let interact = ptr * u.pointer_a.w;
    var along01 = (u.pointer_a.y - g.half_size.y) / max(g.half_size.y, 1.0);
    if (u.glass_rect.z >= u.glass_rect.w) {
        along01 = (u.pointer_a.x - g.half_size.x) / max(g.half_size.x, 1.0);
    }
    let swing = clamp(along01, -1.0, 1.0) * 0.7 * interact;
    let cs = cos(swing);
    let sn = sin(swing);
    let l_dir = vec2<f32>(l0.x * cs - l0.y * sn, l0.x * sn + l0.y * cs);

    let sigma = max(g.min_half * 1.6, 1.0);
    let spot = interact * exp(-dot(dp, dp) / (2.0 * sigma * sigma));

    let sheen = u.surface_a.y;
    if (sheen > 0.0001) {
        let q = g.lp / max(g.half_size, vec2<f32>(1.0));
        let along = dot(vec2<f32>(q.x * 0.35, q.y), l_dir);
        let sh = clamp(sheen * smoothstep(0.0, 1.0, along), 0.0, 1.0);
        glass = over(vec4<f32>(w * sh, sh), glass);
    }
    if (spot > 0.0001) {
        let sp = clamp(0.12 * spot, 0.0, 1.0);
        glass = over(vec4<f32>(w * sp, sp), glass);
    }

    // Dual opposing-corner specular rim (-135 deg + +45 deg) modulated by Apple's vibrantColorMatrix.
    let ndl = dot(g.n, l_dir);
    let key = pow(max(ndl, 0.0), 1.85);
    let fill = pow(max(-ndl, 0.0), 2.05) * 1.05;
    let dir_light = max(key, fill);

    let rim_w = max(u.rim_params.y * 0.85, px);
    let d_rim = (g.depth - 1.05 * px) / rim_w;
    let rim_core = exp(-0.5 * d_rim * d_rim);
    let rim_halo = pow(max(1.0 - g.depth / (rim_w * 5.0), 0.0), 2.4) * 0.12;

    var spec_amt = u.rim_params.x * (dir_light * 0.89 + 0.11) * (rim_core + rim_halo);
    spec_amt = clamp(spec_amt * (1.0 + 1.8 * spot), 0.0, 1.0);

    var unpremul = glass.rgb;
    if (glass.a > 1e-4) {
        unpremul = glass.rgb / glass.a;
    }
    let bg_luma = clamp(dot(unpremul / max(w, vec3<f32>(1e-4)), vec3<f32>(0.2126, 0.7152, 0.0722)), 0.0, 1.0);
    let luma_gain = 1.0 - 0.35 * bg_luma;
    let w_rim = spec_amt * luma_gain;
    let spec_rgb = vibrant_highlight_color(unpremul, w);

    let peak_limit = max(glass.rgb, w);
    glass = vec4<f32>(min(glass.rgb + spec_rgb * (w_rim * 0.50), peak_limit), glass.a);
    let over_spec = w_rim * 0.42;
    glass = over(vec4<f32>(spec_rgb * over_spec, over_spec), glass);

    // Optional film grain.
    let grain = u.surface_a.w;
    if (grain > 0.0) {
        let gr = hash12(floor(in.clip_pos.xy)) - 0.5;
        glass = vec4<f32>(max(glass.rgb + w * (gr * grain) * glass.a, vec3<f32>(0.0)), glass.a);
    }

    // Optional inner hairline.
    let bw = u.surface_a.z;
    let ib = u.inner_border.a * (1.0 - smoothstep(bw - 0.5 * px, bw + 0.5 * px, g.depth));
    glass = over(vec4<f32>(u.inner_border.rgb * ib, ib), glass);

    glass = glass * g.inside;

    // Outside: hairline + soft drop shadow.
    let outside = 1.0 - g.inside;
    let ob_cov = clamp(0.5 - (g.d - bw) / px, 0.0, 1.0) * outside;
    let ob = u.outer_border.a * ob_cov;

    let ds = sd_round_rect(g.lp - u.shadow_a.yz, g.half_size, g.r);
    let sb = max(u.shadow_a.x, 0.001);
    let f = 1.0 - smoothstep(-0.35 * sb, sb, ds);
    let sa = u.shadow_color.a * f * f * outside;

    var under = vec4<f32>(u.shadow_color.rgb * sa, sa);
    under = over(vec4<f32>(u.outer_border.rgb * ob, ob), under);

    var final_c = over(glass, under) * u.pointer_b.z;
    if (u.misc.z < 0.5 && final_c.a > 1e-4) {
        final_c = vec4<f32>(final_c.rgb / final_c.a, final_c.a);
    }
    return final_c;
}

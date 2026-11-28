// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Liquid Glass shading core of the Kogical.LiquidGlass Qt Quick module (inlined before qsb runs).
// The Fluctus Glass KWin effect renders with the Rust engine in crates/liquidglass instead
// (shaders/liquidglass.wgsl); keep the two looks in step when changing either.
//
// Written in a portable GLSL subset (GLSL 4.40 for qsb, also valid GLSL ES 3.00):
// no #version, no uniforms, no layout qualifiers. Hosts fill LGParams from their own uniforms.
//
// Coordinate convention: `p` and every length are in one consistent pixel space (logical px in
// Qt Quick, device px in KWin) with y pointing down; `px` is the size of one device pixel in
// that space (1 / devicePixelRatio in Qt Quick, 1.0 in KWin).

struct LGParams {
    vec4 glassRect;        // glass rect (x, y, w, h)
    float radius;          // corner radius
    float bezel;           // lens bezel width
    float refraction;      // max refraction displacement
    float dispersion;      // chromatic dispersion factor 0..1
    float saturation;      // backdrop saturation multiplier
    float brightness;      // backdrop brightness multiplier
    vec4 tint;             // straight-alpha RGBA
    float rimLight;        // specular rim intensity
    float rimWidth;        // specular rim falloff
    vec2 lightDir;         // direction towards the key light (y down)
    float edgeShade;       // refraction darkening inside the bezel
    float sheen;           // soft highlight on the lit half
    vec4 innerBorder;      // straight-alpha RGBA, just inside the edge
    vec4 outerBorder;      // straight-alpha RGBA, just outside the edge
    float borderWidth;     // hairline width
    float grain;           // film grain 0..1
    vec4 shadowColor;      // straight-alpha RGBA
    float shadowBlur;      // shadow softness
    vec2 shadowOffset;     // shadow offset
    float px;              // one device pixel
    vec2 pointerPos;       // pointer, glass-local
    float pointerStrength; // 0..1 presence
    float pointerLight;    // pointer-driven lighting amount
    float pointerBulge;    // lens bulge displacement under the pointer
    float pointerRadius;   // lens bulge radius
    float magnify;         // whole-body dome magnification 0..0.35 (thick convex slab)
    float edgeBend;        // extra steep outer-lip refraction bend 0..1
    vec3 white;            // reference white of the output (1.0 for SDR)
};

struct LGGeom {
    vec2 lp;          // position relative to the glass centre
    vec2 halfSize;
    float minHalf;    // min(halfSize.x, halfSize.y) for scale-invariant pill/card optics
    float r;          // effective corner radius
    float d;          // signed distance to the edge (negative inside)
    float inside;     // anti-aliased coverage
    float depth;      // distance inside the edge (0 outside)
    vec2 n;           // outward surface normal
    float effBezel;   // bezel width clamped cleanly to shape thickness
    float t;          // 0 at the rim, 1 past the bezel
    float lens;       // inner convex Snell lens profile (1 at rim, C1-smooth to 0 at t=1)
    float outerLens;  // outer lip fold profile (Apple's positive outer refraction zone)
};

/// Continuous-curvature rounded box SDF (superellipse / Lamé corner with n = 2.2 on rounded
/// rects, smoothly relaxing to n = 2.0 on full capsules and circles so circular buttons stay
/// true circles just like Core Animation's `.continuous` corner curve).
float lgSdRoundRect(vec2 p, vec2 halfSize, float r) {
    vec2 q = abs(p) - halfSize + vec2(r);
    if (r > 0.5 && q.x > 0.0 && q.y > 0.0) {
        float minH = max(min(halfSize.x, halfSize.y), 1.0);
        float circleMix = smoothstep(0.76, 0.98, r / minH);
        float n = mix(2.2, 2.0, circleMix);
        vec2 u = q / r;
        float s = pow(pow(u.x, n) + pow(u.y, n), 1.0 / n);
        return (s - 1.0) * r;
    }
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

vec2 lgSdNormal(vec2 p, vec2 halfSize, float r) {
    float e = 0.5;
    vec2 g = vec2(
        lgSdRoundRect(p + vec2(e, 0.0), halfSize, r) - lgSdRoundRect(p - vec2(e, 0.0), halfSize, r),
        lgSdRoundRect(p + vec2(0.0, e), halfSize, r) - lgSdRoundRect(p - vec2(0.0, e), halfSize, r));
    float l = length(g);
    return l > 1e-5 ? g / l : vec2(0.0, -1.0);
}

/// C1-smooth 3D Snell refraction profile (IOR 1.5) for a convex circular bezel
/// h(x) = sqrt(1 - (1 - x)^2), damped smoothly to zero derivative at x = 1 so interior lines
/// curve organically into the bezel without any kink seam.
float lgSnellSquircle(float x) {
    float xc = clamp(x, 0.0, 1.0);
    float a = 1.0 - xc;
    float b = max(1.0 - a * a, 1e-4);
    float height = sqrt(b);
    float smoothTail = 1.0 - xc * xc * (3.0 - 2.0 * xc);
    float slope = min(a / height, 4.5) * smoothTail;
    vec3 N = normalize(vec3(-slope, 0.0, 1.0));
    vec3 T = refract(vec3(0.0, 0.0, -1.0), N, 1.0 / 1.5);
    float lateral = abs(T.x) / max(-T.z, 1e-4);
    float slabH = 0.70;
    return min(lateral * (slabH + height) / 0.72, 1.0);
}

/// Schlick Fresnel approximation for glass (IOR 1.5 => F0 = 0.04).
float lgFresnel(float cosTheta) {
    float c = 1.0 - clamp(cosTheta, 0.0, 1.0);
    float c2 = c * c;
    return 0.04 + 0.96 * c2 * c2 * c;
}

float lgHash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec4 lgOver(vec4 top, vec4 bottom) {
    return top + bottom * (1.0 - top.a);
}

LGGeom lgGeometry(vec2 p, LGParams P) {
    LGGeom g;
    g.halfSize = P.glassRect.zw * 0.5;
    g.minHalf = max(min(g.halfSize.x, g.halfSize.y), 1.0);
    vec2 centre = P.glassRect.xy + g.halfSize;
    g.r = clamp(P.radius, 0.0, g.minHalf);
    g.lp = p - centre;
    g.d = lgSdRoundRect(g.lp, g.halfSize, g.r);
    g.inside = clamp(0.5 - g.d / P.px, 0.0, 1.0);
    g.depth = max(-g.d, 0.0);
    g.n = lgSdNormal(g.lp, g.halfSize, g.r);
    g.effBezel = min(P.bezel, g.minHalf * 0.48);
    g.t = clamp(g.depth / max(g.effBezel, 0.001), 0.0, 1.0);
    g.lens = lgSnellSquircle(g.t);
    // Outer lip zone (~36% of bezel width): Apple's positive outer refraction zone that bends
    // surrounding light tightly around the outer glass wall and drives vivid rim dispersion.
    float outerW = max(g.effBezel * 0.36, 3.0 * P.px);
    float uOut = clamp(g.depth / outerW, 0.0, 1.0);
    g.outerLens = pow(1.0 - uOut, 2.2);
    return g;
}

/// Smooth pillow-dome magnification vector across the glass body. Gently increases towards the
/// bezel so straight background lines bend organically like a thick curved lens, while clamping
/// the lever arm on wide pills/docks so narrow horizontal bars never stretch excessively.
vec2 lgDomeOffset(LGGeom g, LGParams P) {
    if (P.magnify <= 0.0001) {
        return vec2(0.0);
    }
    float domeWall = pow(clamp(1.0 - g.depth / max(g.minHalf * 0.95, 1.0), 0.0, 1.0), 1.6);
    float domeM = P.magnify * (0.78 + 0.52 * domeWall);
    float lpLen = length(g.lp);
    float armScale = min(1.0, (g.minHalf * 1.65) / max(lpLen, 1.0));
    return -g.lp * (domeM * armScale);
}

/// Two-zone curved-bezel refraction displacement (inner convex inward bend + outer lip outward
/// fold, plus pointer bulge). Scaled proportionally to `effBezel` so small pills and circles stay
/// jewel-like without clipping. Chromatic dispersion couples strictly to this curved-glass vector.
vec2 lgLensOffset(LGGeom g, vec2 p, LGParams P) {
    float bezelScale = P.bezel > 0.001 ? clamp(g.effBezel / P.bezel, 0.0, 1.0) : 0.0;
    float effRef = P.refraction * bezelScale;
    // Two-zone opposite-sign refraction (matches Apple's negative inner + positive outer zones):
    // inner zone bends rays inward (-n), outer lip releases the dome pull and folds outward (+n)
    // far enough to catch surrounding highlights into a vivid, jewel-like liquid meniscus.
    vec2 dome = lgDomeOffset(g, P);
    vec2 o = -g.n * (effRef * g.lens)
             + (-dome + g.n * (effRef * (1.65 + 0.70 * P.edgeBend))) * g.outerLens;
    float ptr = clamp(P.pointerStrength, 0.0, 1.0);
    if (ptr > 0.0 && P.pointerBulge > 0.0) {
        vec2 dP = (p - P.glassRect.xy) - P.pointerPos;
        float sigma = max(P.pointerRadius, 1.0);
        float f = exp(-dot(dP, dP) / (2.0 * sigma * sigma));
        // Sample towards the pointer: magnifies what is under it, like a drop of water.
        o -= dP * (P.pointerBulge / sigma) * f * ptr;
    }
    return o;
}

/// Total refraction displacement (same units as p) for the green/centre backdrop lookup:
/// smooth convex pillow dome + two-zone curved bezel refraction.
vec2 lgRefractionOffset(LGGeom g, vec2 p, LGParams P) {
    return lgDomeOffset(g, P) + lgLensOffset(g, p, P);
}

/// Per-channel chromatic dispersion offset (Cauchy: R = -d, B = +d). Strictly coupled to the
/// curved bezel refraction and pointer bulge so the interior stays crisp and color-pure while
/// the steep curved rim splits high-contrast edges into silky cyan & magenta prisms.
vec2 lgDispersionOffset(LGGeom g, vec2 p, LGParams P) {
    return lgLensOffset(g, p, P) * P.dispersion;
}

/// Progressive bezel clarity (0 = diffused interior, ~0.95 = crystal-sharp curved rim).
float lgRimClarity(LGGeom g) {
    return clamp(pow(max(g.lens, g.outerLens), 1.15) * 0.95, 0.0, 0.95);
}

vec3 lgGrade(vec3 bg, LGParams P) {
    float lum = dot(bg, vec3(0.2126, 0.7152, 0.0722));
    vec3 graded = max(mix(vec3(lum), bg, P.saturation) * P.brightness, vec3(0.0));
    // Never push SDR content past reference white; HDR highlights keep their own peak.
    float peak = max(max(bg.r, max(bg.g, bg.b)), max(P.white.r, max(P.white.g, P.white.b)));
    return min(graded, vec3(peak));
}

/// Apple's verified QuartzCore `vibrantColorMatrix` for `CASDFGlassHighlightEffect` passes:
/// lifts saturated backdrop colors into luminous glowing tints on dark glass while staying soft
/// and pearlescent on bright backgrounds (avoiding chalky grey CSS-style outlines).
vec3 lgVibrantHighlightColor(vec3 rgb, vec3 white) {
    vec3 safeW = max(white, vec3(1e-4));
    vec3 c = clamp(rgb / safeW, 0.0, 1.0);
    vec3 vib = vec3(
         2.6705 * c.r - 1.1088 * c.g - 0.1117 * c.b + 0.05,
        -0.3295 * c.r + 1.8914 * c.g - 0.1119 * c.b + 0.05,
        -0.3297 * c.r - 1.1084 * c.g + 2.8881 * c.b + 0.05
    );
    vib = clamp(vib, 0.0, 1.0);
    return (0.50 * vib + 0.50) * white;
}

/// Lighting, borders, coverage and the outside-only shadow on top of a glass body colour
/// (premultiplied). Returns the final premultiplied colour.
vec4 lgShade(vec4 glass, LGGeom g, vec2 p, vec2 fragCoord, LGParams P) {
    vec3 W = P.white;
    float px = P.px;
    vec2 gpx = p - P.glassRect.xy;
    vec2 dP = gpx - P.pointerPos;
    float ptr = clamp(P.pointerStrength, 0.0, 1.0);

    // Optional mid-bezel refractive darkening band (0 in clear Liquid Glass, available for dark glass).
    if (P.edgeShade > 0.0001) {
        float band = sin(3.14159265 * g.t) * (1.0 - g.t * 0.35);
        float shade = clamp(P.edgeShade * band, 0.0, 1.0);
        glass = lgOver(vec4(0.0, 0.0, 0.0, shade), glass);
    }

    // Key light: resting direction (-135 deg top-left), swung along the long axis towards pointer.
    vec2 L0 = normalize(P.lightDir);
    float interact = ptr * P.pointerLight;
    bool horizontal = P.glassRect.z >= P.glassRect.w;
    float along01 = horizontal ? (P.pointerPos.x - g.halfSize.x) / max(g.halfSize.x, 1.0)
                               : (P.pointerPos.y - g.halfSize.y) / max(g.halfSize.y, 1.0);
    float swing = clamp(along01, -1.0, 1.0) * 0.7 * interact;
    float cs = cos(swing);
    float sn = sin(swing);
    vec2 L = vec2(L0.x * cs - L0.y * sn, L0.x * sn + L0.y * cs);

    // Soft light spot following the pointer.
    float sigma = max(g.minHalf * 1.6, 1.0);
    float spot = interact * exp(-dot(dP, dP) / (2.0 * sigma * sigma));

    // Subtle sheen over the lit half of the surface.
    if (P.sheen > 0.0001) {
        vec2 q = g.lp / max(g.halfSize, vec2(1.0));
        float along = dot(vec2(q.x * 0.35, q.y), L);
        float sh = clamp(P.sheen * smoothstep(0.0, 1.0, along), 0.0, 1.0);
        glass = lgOver(vec4(W * sh, sh), glass);
    }
    if (spot > 0.0001) {
        float sp = clamp(0.12 * spot, 0.0, 1.0);
        glass = lgOver(vec4(W * sp, sp), glass);
    }

    // Dual opposing-corner specular rim (Apple's verified -135 deg + +45 deg CASDFGlassHighlightEffect
    // passes paired with vibrantColorMatrix): luminous pearlescent top-left and vibrant cyan/white
    // bottom-right arcs, fading nearly to zero on the cross-diagonal corners so refracted glass shines.
    float ndl = dot(g.n, L);
    float key = pow(max(ndl, 0.0), 1.85);
    float fill = pow(max(-ndl, 0.0), 2.05) * 1.05;
    float dirLight = max(key, fill);

    float rimW = max(P.rimWidth * 0.85, px);
    float dRim = (g.depth - 1.05 * px) / rimW;
    float rimCore = exp(-0.5 * dRim * dRim);
    float rimHalo = pow(max(1.0 - g.depth / (rimW * 5.0), 0.0), 2.4) * 0.12;

    float specAmt = P.rimLight * (dirLight * 0.89 + 0.11) * (rimCore + rimHalo);
    specAmt = clamp(specAmt * (1.0 + 1.8 * spot), 0.0, 1.0);

    // Luminance-adaptive vibrant highlight: shines brightly over rich dark color without
    // clipping or chalking out over bright light backgrounds.
    vec3 unpremul = glass.a > 1e-4 ? (glass.rgb / glass.a) : glass.rgb;
    float bgLuma = clamp(dot(unpremul / max(W, vec3(1e-4)), vec3(0.2126, 0.7152, 0.0722)), 0.0, 1.0);
    float lumaGain = 1.0 - 0.35 * bgLuma;
    float wRim = specAmt * lumaGain;
    vec3 specRgb = lgVibrantHighlightColor(unpremul, W);

    vec3 peakLimit = max(glass.rgb, W);
    glass.rgb = min(glass.rgb + specRgb * (wRim * 0.50), peakLimit);
    float overSpec = wRim * 0.42;
    glass = lgOver(vec4(specRgb * overSpec, overSpec), glass);

    // Film grain (acrylic).
    if (P.grain > 0.0) {
        float gr = lgHash12(floor(fragCoord)) - 0.5;
        glass.rgb = max(glass.rgb + W * (gr * P.grain) * glass.a, vec3(0.0));
    }

    // Inner hairline.
    float ib = P.innerBorder.a * (1.0 - smoothstep(P.borderWidth - 0.5 * px, P.borderWidth + 0.5 * px, g.depth));
    glass = lgOver(vec4(P.innerBorder.rgb * ib, ib), glass);

    glass *= g.inside;

    // Outside: hairline + shadow.
    float outside = 1.0 - g.inside;
    float obCov = clamp(0.5 - (g.d - P.borderWidth) / px, 0.0, 1.0) * outside;
    float ob = P.outerBorder.a * obCov;

    float ds = lgSdRoundRect(g.lp - P.shadowOffset, g.halfSize, g.r);
    float sb = max(P.shadowBlur, 0.001);
    float f = 1.0 - smoothstep(-0.35 * sb, sb, ds);
    float sa = P.shadowColor.a * f * f * outside;

    vec4 under = vec4(P.shadowColor.rgb * sa, sa);
    under = lgOver(vec4(P.outerBorder.rgb * ob, ob), under);

    return lgOver(glass, under);
}

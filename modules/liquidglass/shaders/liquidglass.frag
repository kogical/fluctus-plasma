// SPDX-License-Identifier: MIT
// Kogical.LiquidGlass — rounded-rect glass material.
//
// Renders, in one pass:
//   * an outside-only soft drop shadow
//   * optional dual hairline border (light inner / dark outer)
//   * tinted glass body with a convex "lens" bezel
//   * angle-dependent specular rim lighting (key light + opposite fill light)
//   * refraction darkening band, top sheen and optional film grain
// Interactive layer: a pointer position + strength swings the key light towards the pointer
// and adds a soft light spot under it.
// When a backdrop texture is supplied (hasSource > 0.5) the body samples it with
// lens refraction, chromatic dispersion and saturation/brightness lift, giving true
// in-scene liquid glass. Without one, it renders as an overlay meant to sit on top
// of compositor blur (e.g. KWin blur-behind).

#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 itemSize;       // ShaderEffect size in logical px (glass + shadow extent)
    vec4 glassRect;      // glass rect inside the item (x, y, w, h) in logical px
    float radius;        // corner radius, logical px
    vec4 tint;           // straight-alpha RGBA
    float bezel;         // lens bezel width, px
    float refraction;    // max refraction displacement, px (backdrop mode)
    float dispersion;    // chromatic dispersion factor 0..1 (backdrop mode)
    float saturation;    // backdrop saturation multiplier (backdrop mode)
    float brightness;    // backdrop brightness multiplier (backdrop mode)
    float hasSource;     // > 0.5 when `source` holds the backdrop
    float sourceMargin;  // extra px captured around the glass in `source`
    float rimLight;      // specular rim intensity
    float rimWidth;      // specular rim falloff, px
    vec2 lightDir;       // direction towards the key light (screen space, y down)
    float edgeShade;     // refraction darkening inside the bezel
    float sheen;         // soft highlight on the lit half
    vec4 innerBorder;    // straight-alpha RGBA, drawn just inside the edge
    vec4 outerBorder;    // straight-alpha RGBA, drawn just outside the edge
    float borderWidth;   // hairline width, px
    float grain;         // film grain amount 0..1
    vec4 shadowColor;    // straight-alpha RGBA
    float shadowBlur;    // shadow softness, px
    vec2 shadowOffset;   // shadow offset, px
    float dpr;           // device pixel ratio for crisp anti-aliasing
    vec2 pointerPos;     // pointer in glass-local px
    float pointerStrength; // 0..1, animated in/out by the host
    float pointerLight;  // amount of pointer-driven lighting (spot + light swing)
};

layout(binding = 1) uniform sampler2D source;

float sdRoundRect(vec2 p, vec2 halfSize, float r) {
    vec2 q = abs(p) - halfSize + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

vec2 sdNormal(vec2 p, vec2 halfSize, float r) {
    const float e = 0.5;
    vec2 g = vec2(
        sdRoundRect(p + vec2(e, 0.0), halfSize, r) - sdRoundRect(p - vec2(e, 0.0), halfSize, r),
        sdRoundRect(p + vec2(0.0, e), halfSize, r) - sdRoundRect(p - vec2(0.0, e), halfSize, r));
    float l = length(g);
    return l > 1e-5 ? g / l : vec2(0.0, -1.0);
}

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec4 over(vec4 top, vec4 bottom) {
    return top + bottom * (1.0 - top.a);
}

void main() {
    float px = 1.0 / max(dpr, 1.0);
    vec2 p = qt_TexCoord0 * itemSize;
    vec2 halfSize = glassRect.zw * 0.5;
    vec2 centre = glassRect.xy + halfSize;
    float r = clamp(radius, 0.0, min(halfSize.x, halfSize.y));
    vec2 lp = p - centre;

    float d = sdRoundRect(lp, halfSize, r);
    float inside = clamp(0.5 - d / px, 0.0, 1.0);
    float depth = max(-d, 0.0);
    vec2 n = sdNormal(lp, halfSize, r);

    // Pointer interaction.
    vec2 gpx = p - glassRect.xy;
    vec2 dP = gpx - pointerPos;
    float ptr = clamp(pointerStrength, 0.0, 1.0);

    // Lens bezel profile: 0 at the rim, 1 once past the bezel.
    float t = clamp(depth / max(bezel, 0.001), 0.0, 1.0);
    float rim = 1.0 - t;
    float lens = rim * rim;

    // ---- Glass body -------------------------------------------------------
    vec4 glass;
    if (hasSource > 0.5) {
        vec2 texSize = glassRect.zw + vec2(2.0 * sourceMargin);
        vec2 gp = p - glassRect.xy + vec2(sourceMargin);
        vec2 base = gp / texSize;
        vec2 o = (n * refraction * lens) / texSize;
        float red = texture(source, base + o * (1.0 + dispersion)).r;
        vec4 mid = texture(source, base + o);
        float blue = texture(source, base + o * (1.0 - dispersion)).b;
        vec3 bg = vec3(red, mid.g, blue);
        float lum = dot(bg, vec3(0.2126, 0.7152, 0.0722));
        bg = clamp(mix(vec3(lum), bg, saturation) * brightness, 0.0, 1.0);
        glass = vec4(mix(bg, tint.rgb, tint.a), 1.0);
    } else {
        glass = vec4(tint.rgb * tint.a, tint.a);
    }

    // Light gathered by the lens rim: brighter, denser edge.
    float gather = clamp(rimLight * 0.12 * lens, 0.0, 1.0);
    glass = over(vec4(gather), glass);

    // Refraction darkening band peaking mid-bezel.
    float band = sin(3.14159265 * t) * (1.0 - t * 0.35);
    float shade = clamp(edgeShade * band, 0.0, 1.0);
    glass = over(vec4(0.0, 0.0, 0.0, shade), glass);

    // Key light: resting direction, swung along the long axis towards the pointer.
    vec2 L0 = normalize(lightDir);
    float interact = ptr * pointerLight;
    bool horizontal = glassRect.z >= glassRect.w;
    float along01 = horizontal ? (pointerPos.x - halfSize.x) / max(halfSize.x, 1.0)
                               : (pointerPos.y - halfSize.y) / max(halfSize.y, 1.0);
    float swing = clamp(along01, -1.0, 1.0) * 0.7 * interact;
    float cs = cos(swing);
    float sn = sin(swing);
    vec2 L = vec2(L0.x * cs - L0.y * sn, L0.x * sn + L0.y * cs);

    // Soft light spot following the pointer.
    float sigma = max(min(halfSize.x, halfSize.y) * 1.6, 1.0);
    float spot = interact * exp(-dot(dP, dP) / (2.0 * sigma * sigma));

    // Soft sheen over the lit half of the surface.
    vec2 q = lp / max(halfSize, vec2(1.0));
    float along = dot(vec2(q.x * 0.35, q.y), L);
    float sh = clamp(sheen * smoothstep(0.05, 1.0, along), 0.0, 1.0);
    glass = over(vec4(sh), glass);
    glass = over(vec4(clamp(0.12 * spot, 0.0, 1.0)), glass);

    // Specular rim: key light, opposite fill light and a crisp catch-light hairline.
    float ndl = dot(n, L);
    float key = pow(max(ndl, 0.0), 4.0);
    float fill = pow(max(-ndl, 0.0), 4.0) * 0.55;
    float rimBand = 1.0 - smoothstep(0.0, max(rimWidth, px), depth);
    float spec = rimLight * rimBand * rimBand * (0.06 + 0.85 * key + fill);
    float hair = rimLight * (1.0 - smoothstep(0.0, 1.25 * px + 0.35, depth)) * (0.16 + 0.84 * max(key, fill));
    spec = clamp((spec + hair) * (1.0 + 2.2 * spot), 0.0, 1.0);
    glass = over(vec4(spec), glass);

    // Film grain (acrylic).
    if (grain > 0.0) {
        float g = hash12(floor(gl_FragCoord.xy)) - 0.5;
        glass.rgb = clamp(glass.rgb + vec3(g * grain) * glass.a, 0.0, 1.0);
    }

    // Inner hairline.
    float ib = innerBorder.a * (1.0 - smoothstep(borderWidth - 0.5 * px, borderWidth + 0.5 * px, depth));
    glass = over(vec4(innerBorder.rgb * ib, ib), glass);

    glass *= inside;

    // ---- Outside: hairline + shadow --------------------------------------
    float outside = 1.0 - inside;
    float obCov = clamp(0.5 - (d - borderWidth) / px, 0.0, 1.0) * outside;
    float ob = outerBorder.a * obCov;

    float ds = sdRoundRect(lp - shadowOffset, halfSize, r);
    float sb = max(shadowBlur, 0.001);
    float f = 1.0 - smoothstep(-0.35 * sb, sb, ds);
    float sa = shadowColor.a * f * f * outside;

    vec4 under = vec4(shadowColor.rgb * sa, sa);
    under = over(vec4(outerBorder.rgb * ob, ob), under);

    fragColor = over(glass, under) * qt_Opacity;
}

// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
// Kogical.LiquidGlass — rounded-rect glass material (Qt Quick host).
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
//
// The shading itself lives in liquidglass_core.glsl; CMake inlines it at the #include line
// below before qsb compiles this file.

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
    float magnify;       // whole-body magnification (backdrop mode)
    float edgeBend;      // steep extra bend at the rim (backdrop mode)
};

layout(binding = 1) uniform sampler2D source;

#include "liquidglass_core.glsl"

void main() {
    LGParams P;
    P.glassRect = glassRect;
    P.radius = radius;
    P.bezel = bezel;
    P.refraction = refraction;
    P.dispersion = dispersion;
    P.saturation = saturation;
    P.brightness = brightness;
    P.tint = tint;
    P.rimLight = rimLight;
    P.rimWidth = rimWidth;
    P.lightDir = lightDir;
    P.edgeShade = edgeShade;
    P.sheen = sheen;
    P.innerBorder = innerBorder;
    P.outerBorder = outerBorder;
    P.borderWidth = borderWidth;
    P.grain = grain;
    P.shadowColor = shadowColor;
    P.shadowBlur = shadowBlur;
    P.shadowOffset = shadowOffset;
    P.px = 1.0 / max(dpr, 1.0);
    P.pointerPos = pointerPos;
    P.pointerStrength = pointerStrength;
    P.pointerLight = pointerLight;
    P.pointerBulge = 0.0; // in-app backdrops are static previews; the compositor adds the bulge
    P.pointerRadius = 1.0;
    P.magnify = magnify;
    P.edgeBend = edgeBend;
    P.white = vec3(1.0);

    vec2 p = qt_TexCoord0 * itemSize;
    LGGeom g = lgGeometry(p, P);

    vec4 glass;
    if (hasSource > 0.5) {
        vec2 texSize = glassRect.zw + vec2(2.0 * sourceMargin);
        vec2 gp = p - glassRect.xy + vec2(sourceMargin);
        vec2 base = gp / texSize;
        vec2 o = lgRefractionOffset(g, p, P) / texSize;
        vec2 d = lgDispersionOffset(g, p, P) / texSize;
        // 5-tap spectral Cauchy dispersion (n_B > n_G > n_R): blue refracts further than red.
        vec3 rgb;
        if (dot(d * texSize, d * texSize) > 0.01) {
            vec3 cR  = texture(source, base + o - d).rgb;
            vec3 cRG = texture(source, base + o - d * 0.5).rgb;
            vec3 cG  = texture(source, base + o).rgb;
            vec3 cGB = texture(source, base + o + d * 0.5).rgb;
            vec3 cB  = texture(source, base + o + d).rgb;
            rgb = vec3(
                cR.r * 0.65 + cRG.r * 0.35,
                cRG.g * 0.20 + cG.g * 0.60 + cGB.g * 0.20,
                cGB.b * 0.35 + cB.b * 0.65
            );
        } else {
            rgb = texture(source, base + o).rgb;
        }
        vec3 bg = clamp(lgGrade(rgb, P), 0.0, 1.0);
        glass = vec4(mix(bg, tint.rgb, tint.a), 1.0);
    } else {
        glass = vec4(tint.rgb * tint.a, tint.a);
    }

    fragColor = lgShade(glass, g, p, gl_FragCoord.xy, P) * qt_Opacity;
}

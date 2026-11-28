// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Liquid Glass compositor pass. The look lives in liquidglass_core.glsl (shared with the
// Kogical.LiquidGlass Qt Quick module); this file only samples the real backdrop KWin captured.
// All lengths are device px relative to the capture origin.
#version 140

#include "fluctusglass/liquidglass_core.glsl"

uniform sampler2D sharpTex;     // captured backdrop, device resolution
uniform sampler2D blurTex;      // Dual Kawase level 1 (half resolution)
uniform vec2 texSize;           // capture size, device px
uniform vec4 uvClamp;           // min.xy, max.zw of the fresh backdrop in uv
uniform float useBlur;          // > 0.5: sample blurTex through the final upsample kernel
uniform vec2 blurHalfpixel;
uniform float blurOffset;
uniform float opacity;

uniform vec4 u_glassRect;
uniform float u_radius;
uniform float u_bezel;
uniform float u_refraction;
uniform float u_dispersion;
uniform float u_saturation;
uniform float u_brightness;
uniform vec4 u_tint;
uniform float u_rimLight;
uniform float u_rimWidth;
uniform vec2 u_lightDir;
uniform float u_edgeShade;
uniform float u_sheen;
uniform vec4 u_innerBorder;
uniform vec4 u_outerBorder;
uniform float u_borderWidth;
uniform float u_grain;
uniform vec4 u_shadowColor;
uniform float u_shadowBlur;
uniform vec2 u_shadowOffset;
uniform vec2 u_pointerPos;
uniform float u_pointerStrength;
uniform float u_pointerLight;
uniform float u_pointerBulge;
uniform float u_pointerRadius;
uniform float u_magnify;
uniform float u_edgeBend;
uniform vec3 u_white;

in vec2 localPos;

out vec4 fragColor;

vec2 toUv(vec2 p)
{
    vec2 uv = vec2(p.x / texSize.x, 1.0 - p.y / texSize.y);
    return clamp(uv, uvClamp.xy, uvClamp.zw);
}

vec4 backdropBlur(vec2 p)
{
    vec2 uv = toUv(p);
    if (useBlur < 0.5) {
        return texture(sharpTex, uv);
    }
    vec2 h = blurHalfpixel * blurOffset;
    vec4 sum = texture(blurTex, uv + vec2(-h.x * 2.0, 0.0));
    sum += texture(blurTex, uv + vec2(-h.x, h.y)) * 2.0;
    sum += texture(blurTex, uv + vec2(0.0, h.y * 2.0));
    sum += texture(blurTex, uv + vec2(h.x, h.y)) * 2.0;
    sum += texture(blurTex, uv + vec2(h.x * 2.0, 0.0));
    sum += texture(blurTex, uv + vec2(h.x, -h.y)) * 2.0;
    sum += texture(blurTex, uv + vec2(0.0, -h.y * 2.0));
    sum += texture(blurTex, uv + vec2(-h.x, -h.y)) * 2.0;
    return sum / 12.0;
}

vec4 backdrop(vec2 p, float clarity)
{
    vec4 blurred = backdropBlur(p);
    if (useBlur < 0.5 || clarity <= 0.001) {
        return blurred;
    }
    vec4 sharp = texture(sharpTex, toUv(p));
    return mix(blurred, sharp, clarity);
}

void main(void)
{
    LGParams P;
    P.glassRect = u_glassRect;
    P.radius = u_radius;
    P.bezel = u_bezel;
    P.refraction = u_refraction;
    P.dispersion = u_dispersion;
    P.saturation = u_saturation;
    P.brightness = u_brightness;
    P.tint = u_tint;
    P.rimLight = u_rimLight;
    P.rimWidth = u_rimWidth;
    P.lightDir = u_lightDir;
    P.edgeShade = u_edgeShade;
    P.sheen = u_sheen;
    P.innerBorder = u_innerBorder;
    P.outerBorder = u_outerBorder;
    P.borderWidth = u_borderWidth;
    P.grain = u_grain;
    P.shadowColor = u_shadowColor;
    P.shadowBlur = u_shadowBlur;
    P.shadowOffset = u_shadowOffset;
    P.px = 1.0;
    P.pointerPos = u_pointerPos;
    P.pointerStrength = u_pointerStrength;
    P.pointerLight = u_pointerLight;
    P.pointerBulge = u_pointerBulge;
    P.pointerRadius = u_pointerRadius;
    P.magnify = u_magnify;
    P.edgeBend = u_edgeBend;
    P.white = u_white;

    vec2 p = localPos;
    LGGeom g = lgGeometry(p, P);

    vec4 glass = vec4(0.0);
    if (g.inside > 0.0) {
        vec2 o = lgRefractionOffset(g, p, P);
        vec2 d = lgDispersionOffset(g, p, P);
        // Progressive clarity: diffused interior transitions smoothly to a crystal-sharp bezel.
        float rimClarity = lgRimClarity(g);
        // 5-tap spectral Cauchy dispersion (n_B > n_G > n_R) for smooth prismatic fringes
        // without 3-channel RGB stairstepping.
        vec3 rgb;
        if (dot(d, d) > 0.01) {
            vec3 cR  = backdrop(p + o - d,       rimClarity).rgb;
            vec3 cRG = backdrop(p + o - d * 0.5, rimClarity).rgb;
            vec3 cG  = backdrop(p + o,           rimClarity).rgb;
            vec3 cGB = backdrop(p + o + d * 0.5, rimClarity).rgb;
            vec3 cB  = backdrop(p + o + d,       rimClarity).rgb;
            rgb = vec3(
                cR.r * 0.65 + cRG.r * 0.35,
                cRG.g * 0.20 + cG.g * 0.60 + cGB.g * 0.20,
                cGB.b * 0.35 + cB.b * 0.65
            );
        } else {
            rgb = backdrop(p + o, rimClarity).rgb;
        }
        vec3 bg = lgGrade(rgb, P);
        glass = vec4(mix(bg, u_tint.rgb, u_tint.a), 1.0);
    }

    fragColor = lgShade(glass, g, p, gl_FragCoord.xy, P) * opacity;
}

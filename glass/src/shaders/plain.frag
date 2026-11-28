// SPDX-FileCopyrightText: 2018 Alex Nemeth <alex.nemeth329@gmail.com>
// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: GPL-2.0-or-later
// Plain frosted blur for the parts of a glass window's blur region that are not glass, matching
// KWin's onscreen blur pass so they look exactly like stock blur.
#version 140

uniform sampler2D texUnit;
uniform mat4 colorMatrix;
uniform float offset;
uniform vec2 halfpixel;
uniform float opacity;

in vec2 uv;

out vec4 fragColor;

void main(void)
{
    vec4 sum = texture(texUnit, uv + vec2(-halfpixel.x * 2.0, 0.0) * offset);
    sum += texture(texUnit, uv + vec2(-halfpixel.x, halfpixel.y) * offset) * 2.0;
    sum += texture(texUnit, uv + vec2(0.0, halfpixel.y * 2.0) * offset);
    sum += texture(texUnit, uv + vec2(halfpixel.x, halfpixel.y) * offset) * 2.0;
    sum += texture(texUnit, uv + vec2(halfpixel.x * 2.0, 0.0) * offset);
    sum += texture(texUnit, uv + vec2(halfpixel.x, -halfpixel.y) * offset) * 2.0;
    sum += texture(texUnit, uv + vec2(0.0, -halfpixel.y * 2.0) * offset);
    sum += texture(texUnit, uv + vec2(-halfpixel.x, -halfpixel.y) * offset) * 2.0;

    fragColor = (sum / 12.0) * colorMatrix * opacity;
}

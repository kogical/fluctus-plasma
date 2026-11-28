// SPDX-FileCopyrightText: 2018 Alex Nemeth <alex.nemeth329@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
// Dual Kawase downsample pass (from KWin's blur effect).
#version 140

uniform sampler2D texUnit;
uniform float offset;
uniform vec2 halfpixel;

in vec2 uv;

out vec4 fragColor;

void main(void)
{
    vec4 sum = texture(texUnit, uv) * 4.0;
    sum += texture(texUnit, uv - halfpixel.xy * offset);
    sum += texture(texUnit, uv + halfpixel.xy * offset);
    sum += texture(texUnit, uv + vec2(halfpixel.x, -halfpixel.y) * offset);
    sum += texture(texUnit, uv - vec2(halfpixel.x, -halfpixel.y) * offset);

    fragColor = sum / 8.0;
}

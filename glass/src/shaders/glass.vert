// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: GPL-3.0-or-later
// Positions are in device px relative to the capture origin; the shader works in that space.
#version 140

uniform mat4 modelViewProjectionMatrix;

in vec2 position;
in vec2 texcoord;

out vec2 localPos;

void main(void)
{
    gl_Position = modelViewProjectionMatrix * vec4(position, 0.0, 1.0);
    localPos = position;
}

// SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: GPL-2.0-or-later
// Pass-through vertex shader for the Dual Kawase passes (from KWin's blur effect).
#version 140

uniform mat4 modelViewProjectionMatrix;

in vec2 position;
in vec2 texcoord;

out vec2 uv;

void main(void)
{
    gl_Position = modelViewProjectionMatrix * vec4(position, 0.0, 1.0);
    uv = texcoord;
}

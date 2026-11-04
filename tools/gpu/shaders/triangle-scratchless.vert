// SPDX-License-Identifier: MIT
#version 450
// Same vertices as the fixed 64x64 probe; no dynamically indexed local array.
void main()
{
    float x = gl_VertexIndex == 1 ? 0.75 : -0.75;
    float y = gl_VertexIndex == 2 ? 0.75 : -0.75;
    gl_Position = vec4(x, y, 0.5, 1.0);
}

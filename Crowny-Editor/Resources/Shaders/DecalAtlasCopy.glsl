#lang glsl
#type vertex
#version 450
layout(location = 0) out vec2 uv;
void main()
{
    uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
#type fragment
#version 450
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(binding = 0) uniform sampler2D source;
layout(binding = 1) uniform CopyConstants { vec4 dimensionsAndMip; } c;
void main()
{
    vec2 p = (uv * (c.dimensionsAndMip.xy + 2.0) - 1.0) / c.dimensionsAndMip.xy;
    color = textureLod(source, fract(p), c.dimensionsAndMip.z);
}

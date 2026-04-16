#lang glsl
#type vertex
#version 450

#pragma polygon_mode wireframe
#pragma cull false

layout (location = 0) in vec3 cw_Position;

layout (binding = 0) uniform cw_MVP
{
    mat4 viewProjection;
    mat4 model;
} mvp;

void main()
{
    gl_Position = mvp.viewProjection * mvp.model * vec4(cw_Position, 1.0);
}

#type fragment
#version 450

layout (location = 0) out vec4 outColor;
layout (location = 1) out int outEntity;

void main()
{
    outEntity = 0;
    outColor = vec4(1.0, 1.0, 1.0, 1.0);
}

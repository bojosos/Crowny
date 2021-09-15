#version 450

layout (location = 0) in vec3 inPos;

layout (binding = 0) uniform MVP
{
    mat4 projection;
    mat4 model;
} mvp;

layout (location = 0) out vec3 outUVW;

void main ()
{
    outUVW = inPos;
    gl_Position = mvp.projection * mvp.model * vec4(inPos.xyz, 1.0);
}

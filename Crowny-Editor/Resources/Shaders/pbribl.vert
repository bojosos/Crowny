#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inTangent;

layout (binding = 0) uniform MVP
{
    mat4 projection;
    mat4 view;
    mat4 model;
} mvp;

layout(location = 0) out DATA
{
    vec3 worldPos;
    vec3 normal;
    vec3 tangent;
    vec2 uv;
} vs_out;

void main() 
{
    vec3 locPos = vec3(mvp.model * vec4(inPos, 1.0));
    vs_out.worldPos = locPos;
    vs_out.normal = mat3(mvp.model) * inNormal;
    vs_out.uv = inUV;
    vs_out.tangent = mat3(mvp.model) * inTangent.xyz;
    gl_Position =  mvp.projection * mvp.view * vec4(locPos, 1.0);
}
#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inTangent;

layout (binding = 0) uniform UBO 
{
        mat4 projection;
        mat4 view;
        mat4 model;
} ubo;

layout(location = 0) out DATA
{
    vec3 worldPos;
    vec3 normal;
    vec3 tangent;
    vec2 uv;
} vs_out;

out gl_PerVertex 
{
    vec4 gl_Position;
};

void main() 
{
        vec3 locPos = vec3(ubo.model * vec4(inPos, 1.0));
        vs_out.worldPos = locPos;
        vs_out.normal = mat3(ubo.model) * inNormal;
        vs_out.uv = inUV;
        vs_out.tangent = vec4(mat3(ubo.model) * inTangent.xyz, inTangent.w);
        gl_Position =  ubo.projection * ubo.view * vec4(locPos, 1.0);
}
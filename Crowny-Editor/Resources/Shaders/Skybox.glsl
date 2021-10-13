#type vertex
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


#type fragment
#version 450

layout (location = 0) in vec3 inUVW;
layout (location = 0) out vec4 outColor;

layout (binding = 1) uniform Params
{
    vec4 lights[4];
    float exposure;
    float gamma;
} params;

layout (binding = 2) uniform samplerCube samplerEnv;

vec3 Uncharted2Tonemap(vec3 color)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    float W = 11.2;

    return ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
}

void main()
{
    vec3 color = texture(samplerEnv, inUVW).rgb;

    color = Uncharted2Tonemap(color * params.exposure);
    color = color * (1.0 / Uncharted2Tonemap(vec3(11.2)));
    color = pow(color, vec3(1.0 / params.gamma));

    outColor = vec4(color, 1.0);
}
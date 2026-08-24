#lang glsl
#type compute
#version 450

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform CwGtaoConstants
{
    uvec2 outputSize;
    uvec2 sourceSize;
    float radiusPixels;
    float intensity;
    float depthBias;
    float padding;
} cwGtao;
layout(set = 0, binding = 1) uniform sampler2D cwSceneDepth;
layout(set = 0, binding = 2) uniform sampler2D cwHiZ;
layout(r8, set = 0, binding = 3) writeonly uniform image2D cwAmbientOcclusion;

const ivec2 cwDirections[8] = ivec2[8](
    ivec2(1, 0), ivec2(1, 1), ivec2(0, 1), ivec2(-1, 1),
    ivec2(-1, 0), ivec2(-1, -1), ivec2(0, -1), ivec2(1, -1));

void main()
{
    ivec2 outputPixel = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(outputPixel, ivec2(cwGtao.outputSize))))
        return;
    ivec2 sourcePixel = min(outputPixel * 2 + ivec2(1), ivec2(cwGtao.sourceSize) - 1);
    float centerDepth = texelFetch(cwSceneDepth, sourcePixel, 0).r;
    if (centerDepth <= 0.0)
    {
        imageStore(cwAmbientOcclusion, outputPixel, vec4(1.0));
        return;
    }

    float occlusion = 0.0;
    for (int ring = 1; ring <= 2; ring++)
    {
        float radius = cwGtao.radiusPixels * float(ring) * 0.5;
        for (int direction = 0; direction < 8; direction++)
        {
            ivec2 offset = ivec2(round(vec2(cwDirections[direction]) * radius));
            ivec2 samplePixel = clamp(sourcePixel + offset, ivec2(0), ivec2(cwGtao.sourceSize) - 1);
            float sampleDepth = texelFetch(cwSceneDepth, samplePixel, 0).r;
            float delta = sampleDepth - centerDepth - cwGtao.depthBias;
            occlusion += smoothstep(0.0, max(centerDepth * 0.08, 1e-4), delta);
        }
    }
    float ao = clamp(1.0 - occlusion * (cwGtao.intensity / 16.0), 0.0, 1.0);
    imageStore(cwAmbientOcclusion, outputPixel, vec4(ao));
}

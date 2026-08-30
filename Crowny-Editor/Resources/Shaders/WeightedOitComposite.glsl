#lang glsl
#type compute
#version 450

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform CwWeightedOitConstants
{
    uvec2 resolution;
} cwWeightedOit;
layout(rgba16f, set = 0, binding = 1) uniform image2D cwHdrColor;
layout(set = 0, binding = 2) uniform sampler2D cwAccumulation;
layout(set = 0, binding = 3) uniform sampler2D cwRevealage;

void main()
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(pixel, ivec2(cwWeightedOit.resolution))))
        return;

    vec4 accumulation = texelFetch(cwAccumulation, pixel, 0);
    float revealage = clamp(texelFetch(cwRevealage, pixel, 0).r, 0.0, 1.0);
    if (revealage >= 1.0)
        return;

    vec3 transparentColor = accumulation.rgb / max(accumulation.a, 1e-5);
    if (any(isinf(transparentColor)) || any(isnan(transparentColor)))
        transparentColor = vec3(0.0);
    vec4 sceneColor = imageLoad(cwHdrColor, pixel);
    float coverage = 1.0 - revealage;
    imageStore(cwHdrColor, pixel, vec4(transparentColor * coverage + sceneColor.rgb * revealage, sceneColor.a));
}

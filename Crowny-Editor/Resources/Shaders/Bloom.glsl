#lang glsl
#type compute
#version 450

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform CwBloomConstants
{
    uvec2 outputSize;
    float threshold;
    float knee;
} cwBloom;
layout(set = 0, binding = 1) uniform sampler2D cwSource;
layout(rgba16f, set = 0, binding = 2) writeonly uniform image2D cwBloomOutput;

void main()
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(pixel, ivec2(cwBloom.outputSize))))
        return;
    ivec2 sourceSize = textureSize(cwSource, 0);
    ivec2 center = min(pixel * 2 + ivec2(1), sourceSize - 1);
    vec3 color = vec3(0.0);
    float weight = 0.0;
    for (int y = -2; y <= 2; y++)
    {
        for (int x = -2; x <= 2; x++)
        {
            float sampleWeight = 1.0 / (1.0 + float(x * x + y * y));
            ivec2 samplePixel = clamp(center + ivec2(x, y), ivec2(0), sourceSize - 1);
            color += texelFetch(cwSource, samplePixel, 0).rgb * sampleWeight;
            weight += sampleWeight;
        }
    }
    color /= max(weight, 1e-4);
    float brightness = max(max(color.r, color.g), color.b);
    float soft = clamp((brightness - cwBloom.threshold + cwBloom.knee) / max(2.0 * cwBloom.knee, 1e-4), 0.0, 1.0);
    float contribution = max(brightness - cwBloom.threshold, 0.0) + soft * soft * cwBloom.knee;
    color *= contribution / max(brightness, 1e-4);
    imageStore(cwBloomOutput, pixel, vec4(color, 1.0));
}

#lang glsl
#type compute
#version 450

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform CwTemporalConstants
{
    uvec2 resolution;
    uint historyValid;
    float feedback;
} cwTemporal;
layout(set = 0, binding = 1) uniform sampler2D cwCurrentColor;
layout(set = 0, binding = 2) uniform sampler2D cwSceneDepth;
layout(set = 0, binding = 3) uniform sampler2D cwVelocity;
layout(set = 0, binding = 4) uniform sampler2D cwHistory;
layout(rgba16f, set = 0, binding = 5) writeonly uniform image2D cwResolved;
layout(rgba16f, set = 0, binding = 6) writeonly uniform image2D cwHistoryOutput;

void main()
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(pixel, ivec2(cwTemporal.resolution))))
        return;
    vec2 inverseResolution = 1.0 / vec2(cwTemporal.resolution);
    vec2 uv = (vec2(pixel) + 0.5) * inverseResolution;
    vec3 current = texelFetch(cwCurrentColor, pixel, 0).rgb;
    vec3 minimumColor = current;
    vec3 maximumColor = current;
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            ivec2 samplePixel = clamp(pixel + ivec2(x, y), ivec2(0), ivec2(cwTemporal.resolution) - 1);
            vec3 sampleColor = texelFetch(cwCurrentColor, samplePixel, 0).rgb;
            minimumColor = min(minimumColor, sampleColor);
            maximumColor = max(maximumColor, sampleColor);
        }
    }
    vec2 velocity = texelFetch(cwVelocity, pixel, 0).xy;
    vec2 historyUv = uv - velocity;
    bool usableHistory = cwTemporal.historyValid != 0u && all(greaterThanEqual(historyUv, vec2(0.0))) &&
                         all(lessThanEqual(historyUv, vec2(1.0))) && texelFetch(cwSceneDepth, pixel, 0).r > 0.0;
    vec3 history = texture(cwHistory, historyUv).rgb;
    history = clamp(history, minimumColor, maximumColor);
    float motionRejection = clamp(length(velocity) * float(max(cwTemporal.resolution.x, cwTemporal.resolution.y)), 0.0, 1.0);
    float historyWeight = usableHistory ? cwTemporal.feedback * (1.0 - motionRejection) : 0.0;
    vec3 result = mix(current, history, historyWeight);
    imageStore(cwResolved, pixel, vec4(result, 1.0));
    imageStore(cwHistoryOutput, pixel, vec4(result, 1.0));
}

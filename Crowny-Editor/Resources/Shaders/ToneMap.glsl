#lang glsl
#pragma variation CW_TONEMAP_OBJECT_ID
#pragma depth_read false
#pragma depth_write true
#pragma depth_compare always_pass
#type vertex
#version 450

layout(location = 0) out vec2 cwUv;

void main()
{
    cwUv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(cwUv * 2.0 - 1.0, 0.0, 1.0);
}

#type fragment
#version 450

layout(location = 0) in vec2 cwUv;
layout(set = 0, binding = 0) uniform sampler2D cwHdrColor;
layout(set = 0, binding = 1) uniform CwToneMapConstants
{
    float exposure;
    float bloomIntensity;
    float sharpeningStrength;
    float padding;
} cwToneMap;
#if CW_TONEMAP_OBJECT_ID
layout(set = 0, binding = 2) uniform isampler2D cwObjectIds;
#endif
layout(set = 0, binding = 3) uniform sampler2D cwSceneDepth;
layout(set = 0, binding = 4) uniform sampler2D cwBloom;

layout(location = 0) out vec4 cwColor;
#if CW_TONEMAP_OBJECT_ID
layout(location = 1) out int cwObjectId;
#endif

float finiteOrZero(float value)
{
    return isnan(value) || isinf(value) ? 0.0 : value;
}

vec3 finiteOrZero(vec3 value)
{
    return vec3(finiteOrZero(value.x), finiteOrZero(value.y), finiteOrZero(value.z));
}

vec3 acesFitted(vec3 value)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    value = max(finiteOrZero(value), vec3(0.0));
    vec3 mapped = (value * (a * value + b)) / (value * (c * value + d) + e);
    return clamp(finiteOrZero(mapped), vec3(0.0), vec3(1.0));
}

vec3 sampleToneMapped(ivec2 pixel, ivec2 imageSize)
{
    ivec2 clampedPixel = clamp(pixel, ivec2(0), imageSize - ivec2(1));
    vec2 uv = (vec2(clampedPixel) + vec2(0.5)) / vec2(imageSize);
    vec3 hdr = texelFetch(cwHdrColor, clampedPixel, 0).rgb;
    hdr += texture(cwBloom, uv).rgb * max(finiteOrZero(cwToneMap.bloomIntensity), 0.0);
    return acesFitted(finiteOrZero(hdr) * max(finiteOrZero(cwToneMap.exposure), 0.0));
}

vec3 contrastAdaptiveSharpen(ivec2 pixel, ivec2 imageSize, float strength)
{
    vec3 center = sampleToneMapped(pixel, imageSize);
    if (isnan(strength) || isinf(strength) || strength <= 0.0)
        return center;

    vec3 left = sampleToneMapped(pixel + ivec2(-1, 0), imageSize);
    vec3 right = sampleToneMapped(pixel + ivec2(1, 0), imageSize);
    vec3 above = sampleToneMapped(pixel + ivec2(0, -1), imageSize);
    vec3 below = sampleToneMapped(pixel + ivec2(0, 1), imageSize);
    vec3 average = (left + right + above + below) * 0.25;

    const vec3 luminanceWeights = vec3(0.2126, 0.7152, 0.0722);
    float minimumLuminance = min(dot(center, luminanceWeights),
                                 min(min(dot(left, luminanceWeights), dot(right, luminanceWeights)),
                                     min(dot(above, luminanceWeights), dot(below, luminanceWeights))));
    float maximumLuminance = max(dot(center, luminanceWeights),
                                 max(max(dot(left, luminanceWeights), dot(right, luminanceWeights)),
                                     max(dot(above, luminanceWeights), dot(below, luminanceWeights))));
    float localContrast = maximumLuminance - minimumLuminance;
    float adaptiveStrength = clamp(strength, 0.0, 1.0) * (1.0 - smoothstep(0.35, 1.0, localContrast));
    return clamp(center + (center - average) * adaptiveStrength, vec3(0.0), vec3(1.0));
}

void main()
{
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    ivec2 imageSize = textureSize(cwHdrColor, 0);
    ivec2 colorPixel = clamp(ivec2(cwUv * vec2(imageSize)), ivec2(0), imageSize - ivec2(1));
    cwColor = vec4(contrastAdaptiveSharpen(colorPixel, imageSize, cwToneMap.sharpeningStrength), 1.0);
#if CW_TONEMAP_OBJECT_ID
    cwObjectId = texelFetch(cwObjectIds, pixel, 0).r;
#endif
    gl_FragDepth = texelFetch(cwSceneDepth, pixel, 0).r;
}

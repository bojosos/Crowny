#lang glsl
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
    vec2 padding;
} cwToneMap;
layout(set = 0, binding = 2) uniform isampler2D cwObjectIds;
layout(set = 0, binding = 3) uniform sampler2D cwSceneDepth;
layout(set = 0, binding = 4) uniform sampler2D cwBloom;

layout(location = 0) out vec4 cwColor;
layout(location = 1) out int cwObjectId;

vec3 acesFitted(vec3 value)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((value * (a * value + b)) / (value * (c * value + d) + e), 0.0, 1.0);
}

void main()
{
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec3 hdr = texture(cwHdrColor, cwUv).rgb;
    hdr += texture(cwBloom, cwUv).rgb * max(cwToneMap.bloomIntensity, 0.0);
    hdr *= max(cwToneMap.exposure, 0.0);
    cwColor = vec4(acesFitted(hdr), 1.0);
    cwObjectId = texelFetch(cwObjectIds, pixel, 0).r;
    gl_FragDepth = texelFetch(cwSceneDepth, pixel, 0).r;
}

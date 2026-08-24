#lang glsl
#pragma depth_read true
#pragma depth_write false
#pragma depth_compare equal
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
layout(set = 0, binding = 0) uniform CwSkyConstants
{
    mat4 inverseViewProjection;
    vec4 cameraPositionIntensity;
    vec4 backgroundColor;
} cwSky;
layout(set = 0, binding = 1) uniform samplerCube cwEnvironment;
layout(location = 0) out vec4 cwHdrColor;

void main()
{
    vec2 ndc = cwUv * 2.0 - 1.0;
    vec4 world = cwSky.inverseViewProjection * vec4(ndc, 0.0, 1.0);
    vec3 direction = abs(world.w) > 1e-6
                       ? normalize(world.xyz / world.w - cwSky.cameraPositionIntensity.xyz)
                       : normalize(world.xyz);
    vec3 environment = texture(cwEnvironment, direction).rgb;
    vec3 color = mix(cwSky.backgroundColor.rgb, environment,
                     clamp(cwSky.cameraPositionIntensity.w, 0.0, 1.0));
    cwHdrColor = vec4(color, 1.0);
}

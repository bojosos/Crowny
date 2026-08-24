#lang glsl
#pragma depth_read true
#pragma depth_write true
#pragma depth_compare greater_equal
#type vertex
#version 450

#extension GL_ARB_shader_draw_parameters : enable

struct CwInstanceRecord
{
    vec4 currentRow0;
    vec4 currentRow1;
    vec4 currentRow2;
    vec4 previousRow0;
    vec4 previousRow1;
    vec4 previousRow2;
    vec4 boundingSphere;
    uvec4 draw;
};

layout(location = 0) in vec3 cw_Position;
layout(set = 0, binding = 0) uniform CwDepthView
{
    mat4 viewProjection;
    mat4 previousViewProjection;
} cwView;
layout(std430, set = 0, binding = 1) readonly buffer CwInstanceTable
{
    CwInstanceRecord instances[];
};
layout(std430, set = 0, binding = 2) readonly buffer CwVisibleInstanceIds
{
    uvec2 visibleInstances[];
};

layout(location = 0) out vec4 cwCurrentClip;
layout(location = 1) out vec4 cwPreviousClip;
layout(location = 2) flat out uint cwObjectId;

void main()
{
    CwInstanceRecord instance = instances[visibleInstances[gl_InstanceIndex].x];
    vec4 position = vec4(cw_Position, 1.0);
    vec3 worldPosition = vec3(dot(instance.currentRow0, position), dot(instance.currentRow1, position),
                              dot(instance.currentRow2, position));
    vec3 previousWorldPosition = vec3(dot(instance.previousRow0, position), dot(instance.previousRow1, position),
                                      dot(instance.previousRow2, position));
    cwCurrentClip = cwView.viewProjection * vec4(worldPosition, 1.0);
    cwPreviousClip = cwView.previousViewProjection * vec4(previousWorldPosition, 1.0);
    const uint motionVectorsFlag = 1u << (24u + 3u);
    if ((instance.draw.x & motionVectorsFlag) == 0u)
        cwPreviousClip = cwCurrentClip;
    cwObjectId = instance.draw.w;
    gl_Position = cwCurrentClip;
}

#type fragment
#version 450

layout(location = 0) in vec4 cwCurrentClip;
layout(location = 1) in vec4 cwPreviousClip;
layout(location = 2) flat in uint cwObjectId;
layout(location = 0) out vec2 cwVelocity;
layout(location = 1) out int cwDepthObjectId;

void main()
{
    vec2 currentNdc = cwCurrentClip.xy / max(abs(cwCurrentClip.w), 1e-6);
    vec2 previousNdc = cwPreviousClip.xy / max(abs(cwPreviousClip.w), 1e-6);
    cwVelocity = (currentNdc - previousNdc) * 0.5;
    cwDepthObjectId = int(cwObjectId);
}

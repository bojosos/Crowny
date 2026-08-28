#lang glsl
#pragma depth_read true
#pragma depth_write true
#pragma depth_compare greater_equal
#pragma variation CW_DEPTH_ANIMATED
#pragma variation CW_DEPTH_OBJECT_ID_ONLY
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
#ifdef CW_DEPTH_ANIMATED
layout(location = 1) in vec3 cw_PreviousPosition;
#endif
layout(location = 4) in vec2 cw_TexCoord0;
layout(location = 5) in vec4 cw_Color;
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

layout(location = 0) out vec2 cwUv;
layout(location = 1) flat out uint cwMaterialIndex;
layout(location = 5) out float cwVertexAlpha;
#ifdef CW_DEPTH_OBJECT_ID_ONLY
layout(location = 2) flat out uint cwObjectId;
#else
layout(location = 2) out vec4 cwCurrentClip;
layout(location = 3) out vec4 cwPreviousClip;
layout(location = 4) flat out uint cwObjectId;
#endif

void main()
{
    uvec2 visibleInstance = visibleInstances[gl_InstanceIndex];
    CwInstanceRecord instance = instances[visibleInstance.x];
    vec4 currentPosition = vec4(cw_Position, 1.0);
    vec3 currentWorldPosition = vec3(dot(instance.currentRow0, currentPosition), dot(instance.currentRow1, currentPosition),
                                     dot(instance.currentRow2, currentPosition));
    vec4 currentClip = cwView.viewProjection * vec4(currentWorldPosition, 1.0);

    cwUv = cw_TexCoord0;
    cwMaterialIndex = visibleInstance.y;
    cwVertexAlpha = cw_Color.a;
#ifdef CW_DEPTH_OBJECT_ID_ONLY
    cwObjectId = instance.draw.w;
#else
    vec4 previousPosition = currentPosition;
#ifdef CW_DEPTH_ANIMATED
    previousPosition = vec4(cw_PreviousPosition, 1.0);
#endif
    vec3 previousWorldPosition = vec3(dot(instance.previousRow0, previousPosition), dot(instance.previousRow1, previousPosition),
                                      dot(instance.previousRow2, previousPosition));
    cwCurrentClip = currentClip;
    cwPreviousClip = cwView.previousViewProjection * vec4(previousWorldPosition, 1.0);
    const uint motionVectorsFlag = 1u << (24u + 3u);
    if ((instance.draw.x & motionVectorsFlag) == 0u)
        cwPreviousClip = cwCurrentClip;
    cwObjectId = instance.draw.w;
#endif
    gl_Position = currentClip;
}

#type fragment
#version 450

#extension GL_EXT_nonuniform_qualifier : require

struct CwMaterialRecord
{
    vec4 baseColor;
    vec4 emissiveAlphaCutoff;
    vec4 metallicRoughnessNormalAo;
    uvec4 textureIndices0;
    uvec4 textureIndices1;
    vec4 toonShadowBands;
    vec4 toonSpecular;
    vec4 toonRim;
    vec4 toonControls;
    vec4 toonArtistic;
    vec4 toonPattern;
    vec4 toonOutlineColor;
    vec4 toonOutline;
    vec4 toonStyle;
    uvec4 textureIndices2;
};

layout(location = 0) in vec2 cwUv;
layout(location = 1) flat in uint cwMaterialIndex;
layout(location = 5) in float cwVertexAlpha;
#ifdef CW_DEPTH_OBJECT_ID_ONLY
layout(location = 2) flat in uint cwObjectId;
layout(location = 0) out int cwDepthObjectId;
#else
layout(location = 2) in vec4 cwCurrentClip;
layout(location = 3) in vec4 cwPreviousClip;
layout(location = 4) flat in uint cwObjectId;
layout(location = 0) out vec2 cwVelocity;
layout(location = 1) out int cwDepthObjectId;
#endif
layout(std430, set = 1, binding = 0) readonly buffer CwMaterialTable
{
    CwMaterialRecord materials[];
};
layout(set = 1, binding = 1) uniform sampler2D cwTextures[];

void main()
{
    CwMaterialRecord material = materials[cwMaterialIndex];
    float alpha = texture(cwTextures[nonuniformEXT(material.textureIndices0.x)], cwUv).a * material.baseColor.a * cwVertexAlpha;
    if (alpha < material.emissiveAlphaCutoff.w)
        discard;

#ifdef CW_DEPTH_OBJECT_ID_ONLY
    cwDepthObjectId = int(cwObjectId);
#else
    vec2 currentNdc = cwCurrentClip.xy / max(abs(cwCurrentClip.w), 1e-6);
    vec2 previousNdc = cwPreviousClip.xy / max(abs(cwPreviousClip.w), 1e-6);
    cwVelocity = (currentNdc - previousNdc) * 0.5;
    cwDepthObjectId = int(cwObjectId);
#endif
}

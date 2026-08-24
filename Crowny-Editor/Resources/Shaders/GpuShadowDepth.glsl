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
layout(location = 4) in vec2 cw_TexCoord0;
layout(set = 0, binding = 0) uniform CwShadowView
{
    mat4 viewProjection;
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

void main()
{
    uvec2 visibleInstance = visibleInstances[gl_InstanceIndex];
    CwInstanceRecord instance = instances[visibleInstance.x];
    vec4 position = vec4(cw_Position, 1.0);
    vec3 worldPosition = vec3(dot(instance.currentRow0, position), dot(instance.currentRow1, position),
                              dot(instance.currentRow2, position));
    cwUv = cw_TexCoord0;
    cwMaterialIndex = visibleInstance.y;
    gl_Position = cwView.viewProjection * vec4(worldPosition, 1.0);
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
layout(std430, set = 1, binding = 0) readonly buffer CwMaterialTable
{
    CwMaterialRecord materials[];
};
layout(set = 1, binding = 1) uniform sampler2D cwTextures[];

void main()
{
    CwMaterialRecord material = materials[cwMaterialIndex];
    uint alphaMode = (material.textureIndices1.w >> 8u) & 0xffu;
    if (alphaMode == 1u)
    {
        float alpha = texture(cwTextures[nonuniformEXT(material.textureIndices0.x)], cwUv).a * material.baseColor.a;
        if (alpha < material.emissiveAlphaCutoff.w)
            discard;
    }
}

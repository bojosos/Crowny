#lang glsl
#pragma depth_read true
#pragma depth_write false
#pragma depth_compare greater_equal
#pragma cull front
blend_state {
    enabled = true;
    color = { srca, srcia, add };
    alpha = { one, srcia, add };
};
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
    vec4 toonSilhouette;
    vec4 toonStyle;
    uvec4 textureIndices2;
};

layout(location = 0) in vec3 cw_Position;
layout(location = 1) in vec3 cw_Normal;
layout(location = 4) in vec2 cw_TexCoord0;
layout(location = 5) in vec4 cw_Color;

layout(set = 0, binding = 0) uniform CwViewConstants
{
    mat4 viewProjection;
    mat4 view;
    mat4 inverseViewProjection;
    vec4 cameraPositionPreExposure;
    uvec4 clusterDimensionsAndTileSize;
    vec4 clusterDepthAndViewport;
} cwView;

layout(std430, set = 0, binding = 1) readonly buffer CwInstanceTable
{
    CwInstanceRecord instances[];
};

layout(std430, set = 0, binding = 2) readonly buffer CwVisibleInstanceIds
{
    uvec2 visibleInstances[];
};

layout(std430, set = 1, binding = 0) readonly buffer CwMaterialTable
{
    CwMaterialRecord materials[];
};

layout(location = 0) out CwSilhouetteData
{
    flat vec4 color;
    flat uint enabled;
    vec2 uv;
    float vertexAlpha;
    flat uint materialIndex;
} outputData;

void main()
{
    uvec2 visibleInstance = visibleInstances[gl_InstanceIndex];
    CwMaterialRecord material = materials[visibleInstance.y];
    const uint toonModel = 2u;
    outputData.enabled = ((material.textureIndices1.w & 0xffu) == toonModel && material.toonSilhouette.x > 0.0 &&
                          material.toonOutlineColor.a > 0.0) ? 1u : 0u;
    outputData.color = material.toonOutlineColor;
    outputData.uv = cw_TexCoord0;
    outputData.vertexAlpha = cw_Color.a;
    outputData.materialIndex = visibleInstance.y;
    if (outputData.enabled == 0u)
    {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        return;
    }

    CwInstanceRecord instance = instances[visibleInstance.x];

    vec4 localPosition = vec4(cw_Position, 1.0);
    vec3 worldPosition = vec3(dot(instance.currentRow0, localPosition),
                              dot(instance.currentRow1, localPosition),
                              dot(instance.currentRow2, localPosition));
    mat3 linearTransform = transpose(mat3(instance.currentRow0.xyz, instance.currentRow1.xyz,
                                          instance.currentRow2.xyz));
    vec3 worldNormal = normalize(transpose(inverse(linearTransform)) * cw_Normal);

    vec4 clipPosition = cwView.viewProjection * vec4(worldPosition, 1.0);
    vec4 normalPosition = cwView.viewProjection * vec4(worldPosition + worldNormal, 1.0);
    vec2 clipNdc = clipPosition.xy / max(abs(clipPosition.w), 0.00001);
    vec2 normalNdc = normalPosition.xy / max(abs(normalPosition.w), 0.00001);
    vec2 screenNormal = normalNdc - clipNdc;
    float screenNormalLength = length(screenNormal);
    screenNormal = screenNormalLength > 0.00001 ? screenNormal / screenNormalLength : vec2(0.0);

    float fadeDistance = material.toonOutline.w;
    float cameraDistance = distance(worldPosition, cwView.cameraPositionPreExposure.xyz);
    float distanceFade = fadeDistance > 0.0 ? clamp(1.0 - cameraDistance / fadeDistance, 0.0, 1.0) : 1.0;
    vec2 pixelToNdc = 2.0 / max(cwView.clusterDepthAndViewport.zw, vec2(1.0));
    clipPosition.xy += screenNormal * material.toonSilhouette.x * distanceFade * pixelToNdc * clipPosition.w;

    gl_Position = clipPosition;
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
    vec4 toonSilhouette;
    vec4 toonStyle;
    uvec4 textureIndices2;
};

layout(location = 0) in CwSilhouetteData
{
    flat vec4 color;
    flat uint enabled;
    vec2 uv;
    float vertexAlpha;
    flat uint materialIndex;
} inputData;

layout(std430, set = 1, binding = 0) readonly buffer CwMaterialTable
{
    CwMaterialRecord materials[];
};

layout(set = 1, binding = 1) uniform sampler2D cwTextures[];

layout(location = 0) out vec4 cwHdrColor;

void main()
{
    if (inputData.enabled == 0u)
        discard;
    CwMaterialRecord material = materials[inputData.materialIndex];
    const uint alphaMode = (material.textureIndices1.w >> 8u) & 0xffu;
    const uint maskMode = 1u;
    if (alphaMode == maskMode)
    {
        float alpha = texture(cwTextures[nonuniformEXT(material.textureIndices0.x)], inputData.uv).a *
                      material.baseColor.a * inputData.vertexAlpha;
        if (alpha < material.emissiveAlphaCutoff.w)
            discard;
    }
    cwHdrColor = inputData.color;
}

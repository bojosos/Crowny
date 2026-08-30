#lang glsl
#pragma depth_read true
#pragma depth_write true
#pragma depth_compare greater_equal
#type vertex
#version 450

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
layout(location = 1) in vec3 cw_Normal;
layout(location = 2) in vec3 cw_Tangent;
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

layout(location = 0) out CwGBufferVertexData
{
    vec3 worldPosition;
    vec3 normal;
    vec3 tangent;
    vec2 uv;
    vec4 color;
    flat uint materialIndex;
    flat uint objectId;
} outputData;

void main()
{
    uvec2 visibleInstance = visibleInstances[gl_InstanceIndex];
    CwInstanceRecord instance = instances[visibleInstance.x];
    vec4 position = vec4(cw_Position, 1.0);
    outputData.worldPosition = vec3(dot(instance.currentRow0, position),
                                    dot(instance.currentRow1, position),
                                    dot(instance.currentRow2, position));
    mat3 linearTransform = transpose(mat3(instance.currentRow0.xyz, instance.currentRow1.xyz,
                                          instance.currentRow2.xyz));
    outputData.normal = normalize(transpose(inverse(linearTransform)) * cw_Normal);
    outputData.tangent = normalize(linearTransform * cw_Tangent);
    outputData.uv = cw_TexCoord0;
    outputData.color = cw_Color;
    outputData.materialIndex = visibleInstance.y;
    outputData.objectId = instance.draw.w;
    gl_Position = cwView.viewProjection * vec4(outputData.worldPosition, 1.0);
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

layout(location = 0) in CwGBufferVertexData
{
    vec3 worldPosition;
    vec3 normal;
    vec3 tangent;
    vec2 uv;
    vec4 color;
    flat uint materialIndex;
    flat uint objectId;
} inputData;

layout(set = 0, binding = 0) uniform CwViewConstants
{
    mat4 viewProjection;
    mat4 view;
    mat4 inverseViewProjection;
    vec4 cameraPositionPreExposure;
    uvec4 clusterDimensionsAndTileSize;
    vec4 clusterDepthAndViewport;
} cwView;

layout(std430, set = 1, binding = 0) readonly buffer CwMaterialTable
{
    CwMaterialRecord materials[];
};
layout(set = 1, binding = 1) uniform sampler2D cwTextures[];

layout(location = 0) out vec4 cwBaseColorAo;
layout(location = 1) out vec4 cwNormalRoughnessMetallic;
layout(location = 2) out vec4 cwEmissive;
layout(location = 3) out int cwMaterialFlags;
layout(location = 4) out int cwObjectId;

vec2 encodeOctahedral(vec3 normal)
{
    normal /= abs(normal.x) + abs(normal.y) + abs(normal.z);
    vec2 encoded = normal.xy;
    if (normal.z < 0.0)
        encoded = (1.0 - abs(encoded.yx)) * sign(encoded.xy);
    return encoded * 0.5 + 0.5;
}

float sampleToonPattern(CwMaterialRecord material, vec3 worldPosition, vec3 normal)
{
    uint textureIndex = material.textureIndices2.x;
    uint mapping = material.textureIndices2.w;
    float scale = material.toonPattern.x;
    float signal;
    if (mapping == 1u)
    {
        vec3 weights = abs(normal);
        weights /= max(weights.x + weights.y + weights.z, 1e-5);
        float x = texture(cwTextures[nonuniformEXT(textureIndex)], worldPosition.yz * scale).r;
        float y = texture(cwTextures[nonuniformEXT(textureIndex)], worldPosition.xz * scale).r;
        float z = texture(cwTextures[nonuniformEXT(textureIndex)], worldPosition.xy * scale).r;
        signal = dot(vec3(x, y, z), weights);
    }
    else if (mapping == 2u)
    {
        vec2 screenUv = gl_FragCoord.xy / cwView.clusterDepthAndViewport.zw;
        signal = texture(cwTextures[nonuniformEXT(textureIndex)], screenUv * scale).r;
    }
    else
        signal = texture(cwTextures[nonuniformEXT(textureIndex)], inputData.uv * scale).r;
    float fadeDistance = material.toonPattern.w;
    float distanceFade = fadeDistance <= 0.0 ? 1.0 :
        1.0 - smoothstep(fadeDistance * 0.75, fadeDistance,
                         length(cwView.cameraPositionPreExposure.xyz - worldPosition));
    return mix(0.5, signal, distanceFade);
}

void main()
{
    CwMaterialRecord material = materials[inputData.materialIndex];
    vec4 baseColor = texture(cwTextures[nonuniformEXT(material.textureIndices0.x)], inputData.uv) *
                     material.baseColor * inputData.color;
    uint alphaMode = (material.textureIndices1.w >> 8u) & 0xffu;
    if (alphaMode == 1u && baseColor.a < material.emissiveAlphaCutoff.w)
        discard;

    vec3 geometricNormal = normalize(inputData.normal);
    vec3 tangent = normalize(inputData.tangent - geometricNormal * dot(inputData.tangent, geometricNormal));
    vec3 bitangent = normalize(cross(geometricNormal, tangent));
    vec3 tangentNormal = texture(cwTextures[nonuniformEXT(material.textureIndices0.y)], inputData.uv).xyz * 2.0 - 1.0;
    tangentNormal.xy *= material.metallicRoughnessNormalAo.z;
    vec3 normal = normalize(mat3(tangent, bitangent, geometricNormal) * tangentNormal);
    vec4 metallicRoughness = texture(cwTextures[nonuniformEXT(material.textureIndices0.z)], inputData.uv);
    float ao = texture(cwTextures[nonuniformEXT(material.textureIndices0.w)], inputData.uv).r *
               material.metallicRoughnessNormalAo.w;

    cwBaseColorAo = vec4(max(baseColor.rgb, vec3(0.0)), clamp(ao, 0.0, 1.0));
    cwNormalRoughnessMetallic = vec4(encodeOctahedral(normal),
        clamp(metallicRoughness.g * material.metallicRoughnessNormalAo.y, 0.045, 1.0),
        clamp(metallicRoughness.b * material.metallicRoughnessNormalAo.x, 0.0, 1.0));
    float patternSignal = (material.textureIndices1.w & 0xffu) == 2u ?
        sampleToonPattern(material, inputData.worldPosition, normal) : 0.5;
    cwEmissive = vec4(texture(cwTextures[nonuniformEXT(material.textureIndices1.x)], inputData.uv).rgb *
                      material.emissiveAlphaCutoff.rgb, patternSignal);
    cwMaterialFlags = int(inputData.materialIndex);
    cwObjectId = int(inputData.objectId);
}

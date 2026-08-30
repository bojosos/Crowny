#lang glsl
#pragma depth_read true
#pragma depth_write true
#pragma depth_compare greater_equal
#pragma variation CW_WEIGHTED_OIT_ACCUMULATION
#pragma variation CW_WEIGHTED_OIT_REVEALAGE
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

layout(location = 0) out CwVertexData
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
    uint instanceIndex = visibleInstance.x;
    CwInstanceRecord instance = instances[instanceIndex];
    vec4 position = vec4(cw_Position, 1.0);
    outputData.worldPosition = vec3(dot(instance.currentRow0, position),
                                    dot(instance.currentRow1, position),
                                    dot(instance.currentRow2, position));
    mat3 linearTransform = transpose(mat3(instance.currentRow0.xyz, instance.currentRow1.xyz,
                                          instance.currentRow2.xyz));
    mat3 normalTransform = transpose(inverse(linearTransform));
    outputData.normal = normalize(normalTransform * cw_Normal);
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

#include "CrownyPbrLighting.glslinc"
#include "CrownyClusteredLighting.glslinc"
#include "CrownyShadowTypes.glslinc"

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

layout(location = 0) in CwVertexData
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

layout(std430, set = 0, binding = 3) readonly buffer CwLightTable
{
    CwLightRecord lights[];
};
layout(std430, set = 0, binding = 4) readonly buffer CwClusterCells
{
    CwClusterCell clusterCells[];
};
layout(std430, set = 0, binding = 5) readonly buffer CwClusterLightIndices
{
    uint clusterLightIndices[];
};
layout(std430, set = 0, binding = 6) readonly buffer CwDirectionalLightIndices
{
    uint directionalLightIndices[];
};
layout(std430, set = 0, binding = 7) readonly buffer CwClusterCounters
{
    uint lightIndexCount;
    uint directionalCount;
    uint overflowCount;
    uint counterPadding;
} cwClusterCounters;
layout(set = 0, binding = 8) uniform CwEnvironment
{
    vec4 diffuseSh[9];
    float specularMipCount;
    float intensity;
    vec2 padding;
} cwEnvironment;
layout(set = 0, binding = 9) uniform samplerCube cwPrefilteredEnvironment;
layout(set = 0, binding = 10) uniform sampler2D cwBrdfLut;
layout(std430, set = 0, binding = 11) readonly buffer CwShadowLightTable
{
    CwShadowLightRecord shadowLights[];
};
layout(std430, set = 0, binding = 12) readonly buffer CwShadowViewTable
{
    CwShadowViewRecord shadowViews[];
};
layout(set = 0, binding = 13) uniform sampler2DShadow cwShadowAtlas;
layout(set = 0, binding = 14) uniform samplerCubeArrayShadow cwPointShadows;
layout(set = 0, binding = 15) uniform sampler2DArrayShadow cwDirectionalShadows;
layout(set = 0, binding = 16) uniform sampler2D cwScreenAmbientOcclusion;

#include "CrownyShadowSampling.glslinc"

layout(std430, set = 1, binding = 0) readonly buffer CwMaterialTable
{
    CwMaterialRecord materials[];
};
layout(set = 1, binding = 1) uniform sampler2D cwTextures[];

layout(location = 0) out vec4 cwHdrColor;
#if !defined(CW_WEIGHTED_OIT_ACCUMULATION) && !defined(CW_WEIGHTED_OIT_REVEALAGE)
layout(location = 1) out int cwMaterialId;
layout(location = 2) out int cwObjectId;
#endif

vec3 sampleNormal(CwMaterialRecord material, vec3 geometricNormal)
{
    vec3 tangentNormal = texture(cwTextures[nonuniformEXT(material.textureIndices0.y)], inputData.uv).xyz * 2.0 - 1.0;
    tangentNormal.xy *= material.metallicRoughnessNormalAo.z;
    vec3 tangent = normalize(inputData.tangent - geometricNormal * dot(inputData.tangent, geometricNormal));
    vec3 bitangent = normalize(cross(geometricNormal, tangent));
    return normalize(mat3(tangent, bitangent, geometricNormal) * tangentNormal);
}

float sampleToonPattern(CwMaterialRecord material, CwPbrSurface surface, vec2 screenUv)
{
    uint textureIndex = material.textureIndices2.x;
    uint mapping = material.textureIndices2.w;
    float scale = material.toonPattern.x;
    float signal;
    if (mapping == 1u)
    {
        vec3 weights = abs(surface.normal);
        weights /= max(weights.x + weights.y + weights.z, 1e-5);
        float x = texture(cwTextures[nonuniformEXT(textureIndex)], surface.position.yz * scale).r;
        float y = texture(cwTextures[nonuniformEXT(textureIndex)], surface.position.xz * scale).r;
        float z = texture(cwTextures[nonuniformEXT(textureIndex)], surface.position.xy * scale).r;
        signal = dot(vec3(x, y, z), weights);
    }
    else if (mapping == 2u)
        signal = texture(cwTextures[nonuniformEXT(textureIndex)], screenUv * scale).r;
    else
        signal = texture(cwTextures[nonuniformEXT(textureIndex)], inputData.uv * scale).r;

    float fadeDistance = material.toonPattern.w;
    float distanceFade = fadeDistance <= 0.0 ? 1.0 :
        1.0 - smoothstep(fadeDistance * 0.75, fadeDistance,
                         length(cwView.cameraPositionPreExposure.xyz - surface.position));
    return mix(0.5, signal, distanceFade);
}

vec3 sampleToonRamp(CwMaterialRecord material, CwPbrSurface surface, CwLightRecord light)
{
    float coordinate = clamp(cwToonRampCoordinate(surface, light) + material.toonStyle.y, 0.0, 1.0);
    return texture(cwTextures[nonuniformEXT(material.textureIndices2.y)], vec2(coordinate, 0.5)).rgb;
}

vec3 sampleToonMatcap(CwMaterialRecord material, vec3 worldNormal)
{
    vec2 coordinate = normalize(mat3(cwView.view) * worldNormal).xy;
    float cosine = cos(material.toonStyle.w);
    float sine = sin(material.toonStyle.w);
    coordinate = mat2(cosine, -sine, sine, cosine) * coordinate;
    return texture(cwTextures[nonuniformEXT(material.textureIndices2.z)], coordinate * 0.5 + 0.5).rgb;
}

void writeFragment(vec3 color, float alpha, uint alphaMode)
{
    alpha = clamp(alpha, 0.0, 1.0);
#ifdef CW_WEIGHTED_OIT_ACCUMULATION
    // McGuire/Bavoil weighted blended OIT, adjusted for Crowny's reverse-Z depth.
    float alphaWeight = pow(min(1.0, alpha * 10.0) + 0.01, 3.0);
    float depthWeight = pow(0.1 + gl_FragCoord.z * 0.9, 3.0);
    float weight = clamp(alphaWeight * 1e8 * depthWeight, 1e-2, 3e3);
    cwHdrColor = vec4(color * alpha, alpha) * weight;
#elif defined(CW_WEIGHTED_OIT_REVEALAGE)
    cwHdrColor = vec4(alpha);
#else
    if (alphaMode >= 2u)
        color *= alpha;
    cwHdrColor = vec4(color, alpha);
    cwMaterialId = int(inputData.materialIndex);
    cwObjectId = int(inputData.objectId);
#endif
}

void main()
{
    CwMaterialRecord material = materials[inputData.materialIndex];
    vec4 baseSample = texture(cwTextures[nonuniformEXT(material.textureIndices0.x)], inputData.uv);
    vec4 baseColor = baseSample * material.baseColor * inputData.color;
    uint alphaMode = (material.textureIndices1.w >> 8u) & 0xffu;
    if (alphaMode == 1u && baseColor.a < material.emissiveAlphaCutoff.w)
        discard;

    vec4 metallicRoughness = texture(cwTextures[nonuniformEXT(material.textureIndices0.z)], inputData.uv);
    float ambientOcclusion = texture(cwTextures[nonuniformEXT(material.textureIndices0.w)], inputData.uv).r *
                             material.metallicRoughnessNormalAo.w;
    CwPbrSurface surface;
    surface.position = inputData.worldPosition;
    surface.normal = sampleNormal(material, normalize(inputData.normal));
    surface.view = cwSafeNormalize(cwView.cameraPositionPreExposure.xyz - surface.position);
    surface.baseColor = max(baseColor.rgb, vec3(0.0));
    surface.roughness = clamp(metallicRoughness.g * material.metallicRoughnessNormalAo.y, 0.045, 1.0);
    surface.metallic = clamp(metallicRoughness.b * material.metallicRoughnessNormalAo.x, 0.0, 1.0);
    vec2 screenUv = gl_FragCoord.xy / cwView.clusterDepthAndViewport.zw;
    surface.ambientOcclusion = ambientOcclusion * texture(cwScreenAmbientOcclusion, screenUv).r;
    uint model = material.textureIndices1.w & 0xffu;
    float preExposure = cwView.cameraPositionPreExposure.w;
    vec3 emissive = texture(cwTextures[nonuniformEXT(material.textureIndices1.x)], inputData.uv).rgb *
                    material.emissiveAlphaCutoff.rgb * preExposure;
    if (model == 1u)
    {
        vec3 unlit = surface.baseColor * preExposure + emissive;
        writeFragment(unlit, baseColor.a, alphaMode);
        return;
    }

    float viewDepth = -(cwView.view * vec4(surface.position, 1.0)).z;
    uint clusterIndex = cwClusterIndex(uvec2(gl_FragCoord.xy), viewDepth, cwView.clusterDimensionsAndTileSize.xyz,
                                       cwView.clusterDimensionsAndTileSize.w, cwView.clusterDepthAndViewport.x,
                                       cwView.clusterDepthAndViewport.y);
    CwClusterCell cell = clusterCells[clusterIndex];
    float patternSignal = model == 2u ? sampleToonPattern(material, surface, screenUv) : 0.5;
    vec3 directLighting = vec3(0.0);
    for (uint index = 0u; index < min(cwClusterCounters.directionalCount, 8u); index++)
    {
        uint lightIndex = directionalLightIndices[index];
        float shadow = cwEvaluateShadow(lightIndex, lights[lightIndex], viewDepth, surface.position, surface.normal);
        if (model == 2u)
        {
            vec3 ramp = sampleToonRamp(material, surface, lights[lightIndex]);
            directLighting += cwEvaluateToonDirectLight(surface, lights[lightIndex], shadow, preExposure,
                material.toonShadowBands, material.toonSpecular, material.toonRim, material.toonControls,
                material.toonArtistic, material.toonPattern, patternSignal, ramp, material.toonStyle.x);
        }
        else
            directLighting += cwEvaluateDirectLight(surface, lights[lightIndex], shadow, preExposure);
    }
    for (uint index = 0u; index < cell.count; index++)
    {
        uint lightIndex = clusterLightIndices[cell.offset + index];
        float shadow = cwEvaluateShadow(lightIndex, lights[lightIndex], viewDepth, surface.position, surface.normal);
        if (model == 2u)
        {
            vec3 ramp = sampleToonRamp(material, surface, lights[lightIndex]);
            directLighting += cwEvaluateToonDirectLight(surface, lights[lightIndex], shadow, preExposure,
                material.toonShadowBands, material.toonSpecular, material.toonRim, material.toonControls,
                material.toonArtistic, material.toonPattern, patternSignal, ramp, material.toonStyle.x);
        }
        else
            directLighting += cwEvaluateDirectLight(surface, lights[lightIndex], shadow, preExposure);
    }

    vec3 reflectance = mix(vec3(0.04), surface.baseColor, surface.metallic);
    float normalView = max(dot(surface.normal, surface.view), 0.0);
    vec3 fresnel = reflectance + (max(vec3(1.0 - surface.roughness), reflectance) - reflectance) *
                                  pow(1.0 - normalView, 5.0);
    vec3 diffuse = cwEvaluateDiffuseSh(surface.normal, cwEnvironment.diffuseSh) * surface.baseColor;
    vec3 reflection = reflect(-surface.view, surface.normal);
    vec3 prefiltered = textureLod(cwPrefilteredEnvironment, reflection,
                                  surface.roughness * cwEnvironment.specularMipCount).rgb;
    vec2 brdf = texture(cwBrdfLut, vec2(normalView, surface.roughness)).rg;
    vec3 specular = prefiltered * (fresnel * brdf.x + brdf.y);
    vec3 diffuseWeight = (1.0 - fresnel) * (1.0 - surface.metallic);
    vec3 ambient = (diffuseWeight * diffuse + specular) * surface.ambientOcclusion * cwEnvironment.intensity * preExposure;
    if (model == 2u)
        ambient = diffuse * surface.ambientOcclusion * cwEnvironment.intensity * preExposure * material.toonArtistic.w;

    vec3 outputColor = directLighting + ambient + emissive;
    if (model == 2u && material.toonStyle.z > 0.0)
    {
        vec3 matcap = sampleToonMatcap(material, surface.normal);
        vec3 matcapColor = surface.baseColor * matcap * preExposure + emissive;
        outputColor = mix(outputColor, matcapColor, material.toonStyle.z);
    }
    writeFragment(outputColor, baseColor.a, alphaMode);
}

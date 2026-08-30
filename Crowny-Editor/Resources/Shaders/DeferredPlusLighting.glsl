#lang glsl
#type compute
#version 450

#extension GL_EXT_nonuniform_qualifier : require

#include "CrownyPbrLighting.glslinc"
#include "CrownyClusteredLighting.glslinc"
#include "CrownyShadowTypes.glslinc"

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

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

#include "CrownyShadowSampling.glslinc"

layout(set = 0, binding = 16) uniform sampler2D cwGBufferBaseColorAo;
layout(set = 0, binding = 17) uniform sampler2D cwGBufferNormalRoughnessMetallic;
layout(set = 0, binding = 18) uniform sampler2D cwGBufferEmissive;
layout(set = 0, binding = 19) uniform isampler2D cwGBufferMaterialFlags;
layout(set = 0, binding = 20) uniform sampler2D cwSceneDepth;
layout(rgba16f, set = 0, binding = 21) uniform writeonly image2D cwHdrOutput;
layout(set = 0, binding = 22) uniform sampler2D cwScreenAmbientOcclusion;

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

layout(std430, set = 1, binding = 0) readonly buffer CwMaterialTable
{
    CwMaterialRecord materials[];
};
layout(set = 1, binding = 1) uniform sampler2D cwTextures[];

vec3 decodeOctahedral(vec2 encoded)
{
    vec2 value = encoded * 2.0 - 1.0;
    vec3 normal = vec3(value, 1.0 - abs(value.x) - abs(value.y));
    if (normal.z < 0.0)
        normal.xy = (1.0 - abs(normal.yx)) * sign(normal.xy);
    return normalize(normal);
}

vec3 reconstructWorldPosition(ivec2 pixel, float depth, ivec2 dimensions)
{
    vec2 uv = (vec2(pixel) + 0.5) / vec2(dimensions);
    vec2 ndc = uv * 2.0 - 1.0;
    vec4 world = cwView.inverseViewProjection * vec4(ndc, depth, 1.0);
    return world.xyz / max(abs(world.w), 1e-7) * sign(world.w);
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

void main()
{
    ivec2 dimensions = imageSize(cwHdrOutput);
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(pixel, dimensions)))
        return;

    vec4 baseColorAo = texelFetch(cwGBufferBaseColorAo, pixel, 0);
    vec4 normalRoughnessMetallic = texelFetch(cwGBufferNormalRoughnessMetallic, pixel, 0);
    vec4 emissivePattern = texelFetch(cwGBufferEmissive, pixel, 0);
    vec3 emissive = emissivePattern.rgb;
    int encodedMaterialIndex = texelFetch(cwGBufferMaterialFlags, pixel, 0).r;
    float depth = texelFetch(cwSceneDepth, pixel, 0).r;
    if (depth <= 0.0)
    {
        imageStore(cwHdrOutput, pixel, vec4(0.0));
        return;
    }
    vec3 worldPosition = reconstructWorldPosition(pixel, depth, dimensions);
    if (encodedMaterialIndex < 0 || uint(encodedMaterialIndex) >= uint(materials.length()))
    {
        imageStore(cwHdrOutput, pixel, vec4(emissive, 1.0));
        return;
    }
    CwMaterialRecord material = materials[uint(encodedMaterialIndex)];

    CwPbrSurface surface;
    surface.position = worldPosition;
    surface.normal = decodeOctahedral(normalRoughnessMetallic.xy);
    surface.view = cwSafeNormalize(cwView.cameraPositionPreExposure.xyz - worldPosition);
    surface.baseColor = baseColorAo.rgb;
    vec2 screenUv = (vec2(pixel) + 0.5) / vec2(dimensions);
    surface.ambientOcclusion = baseColorAo.a * texture(cwScreenAmbientOcclusion, screenUv).r;
    surface.roughness = normalRoughnessMetallic.z;
    surface.metallic = normalRoughnessMetallic.w;
    float preExposure = cwView.cameraPositionPreExposure.w;

    uint model = material.textureIndices1.w & 0xffu;
    if (model == 1u)
    {
        imageStore(cwHdrOutput, pixel, vec4(surface.baseColor + emissive * preExposure, 1.0));
        return;
    }

    float viewDepth = -(cwView.view * vec4(worldPosition, 1.0)).z;
    uint clusterIndex = cwClusterIndex(uvec2(pixel), viewDepth, cwView.clusterDimensionsAndTileSize.xyz,
                                       cwView.clusterDimensionsAndTileSize.w, cwView.clusterDepthAndViewport.x,
                                       cwView.clusterDepthAndViewport.y);
    CwClusterCell cell = clusterCells[clusterIndex];
    vec3 directLighting = vec3(0.0);
    for (uint index = 0u; index < min(cwClusterCounters.directionalCount, 8u); index++)
    {
        uint lightIndex = directionalLightIndices[index];
        float shadow = cwEvaluateShadow(lightIndex, lights[lightIndex], viewDepth, worldPosition, surface.normal);
        if (model == 2u)
        {
            vec3 ramp = sampleToonRamp(material, surface, lights[lightIndex]);
            directLighting += cwEvaluateToonDirectLight(surface, lights[lightIndex], shadow, preExposure,
                material.toonShadowBands, material.toonSpecular, material.toonRim, material.toonControls,
                material.toonArtistic, material.toonPattern, emissivePattern.a, ramp, material.toonStyle.x);
        }
        else
            directLighting += cwEvaluateDirectLight(surface, lights[lightIndex], shadow, preExposure);
    }
    for (uint index = 0u; index < cell.count; index++)
    {
        uint lightIndex = clusterLightIndices[cell.offset + index];
        float shadow = cwEvaluateShadow(lightIndex, lights[lightIndex], viewDepth, worldPosition, surface.normal);
        if (model == 2u)
        {
            vec3 ramp = sampleToonRamp(material, surface, lights[lightIndex]);
            directLighting += cwEvaluateToonDirectLight(surface, lights[lightIndex], shadow, preExposure,
                material.toonShadowBands, material.toonSpecular, material.toonRim, material.toonControls,
                material.toonArtistic, material.toonPattern, emissivePattern.a, ramp, material.toonStyle.x);
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
    vec3 outputColor = directLighting + ambient + emissive * preExposure;
    if (model == 2u && material.toonStyle.z > 0.0)
    {
        vec3 matcap = sampleToonMatcap(material, surface.normal);
        vec3 matcapColor = surface.baseColor * matcap * preExposure + emissive * preExposure;
        outputColor = mix(outputColor, matcapColor, material.toonStyle.z);
    }
    imageStore(cwHdrOutput, pixel, vec4(outputColor, 1.0));
}

#lang glsl
#type compute
#version 450

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform CwToonOutlineConstants
{
    mat4 inverseViewProjection;
    vec4 cameraPosition;
    uvec4 resolutionAndFlags;
} cwOutline;
layout(set = 0, binding = 1) uniform sampler2D cwSceneDepth;
layout(set = 0, binding = 2) uniform isampler2D cwMaterialId;
layout(set = 0, binding = 3) uniform sampler2D cwNormalRoughnessMetallic;

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

layout(std430, set = 0, binding = 4) readonly buffer CwMaterialTable
{
    CwMaterialRecord materials[];
};
layout(rgba16f, set = 0, binding = 5) uniform image2D cwHdrColor;

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
    vec4 world = cwOutline.inverseViewProjection * vec4(uv * 2.0 - 1.0, depth, 1.0);
    return world.xyz / max(abs(world.w), 1e-7) * sign(world.w);
}

void main()
{
    ivec2 dimensions = ivec2(cwOutline.resolutionAndFlags.xy);
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(pixel, dimensions)))
        return;

    int materialIndex = texelFetch(cwMaterialId, pixel, 0).r;
    if (materialIndex < 0 || uint(materialIndex) >= uint(materials.length()))
        return;
    CwMaterialRecord material = materials[uint(materialIndex)];
    if ((material.textureIndices1.w & 0xffu) != 2u || material.toonOutline.x <= 0.0 ||
        material.toonOutlineColor.a <= 0.0)
        return;

    float centerDepth = texelFetch(cwSceneDepth, pixel, 0).r;
    if (centerDepth <= 0.0)
        return;
    vec3 centerNormal = cwOutline.resolutionAndFlags.z != 0u
                          ? decodeOctahedral(texelFetch(cwNormalRoughnessMetallic, pixel, 0).xy)
                          : vec3(0.0, 0.0, 1.0);
    int radius = int(clamp(ceil(material.toonOutline.x), 1.0, 4.0));
    float edge = 0.0;
    for (int y = -4; y <= 4; ++y)
    {
        for (int x = -4; x <= 4; ++x)
        {
            if ((x == 0 && y == 0) || abs(x) > radius || abs(y) > radius)
                continue;
            ivec2 neighbor = clamp(pixel + ivec2(x, y), ivec2(0), dimensions - 1);
            int neighborMaterial = texelFetch(cwMaterialId, neighbor, 0).r;
            edge = max(edge, neighborMaterial != materialIndex ? 1.0 : 0.0);
            float neighborDepth = texelFetch(cwSceneDepth, neighbor, 0).r;
            float depthDifference = abs(centerDepth - neighborDepth);
            float depthThreshold = material.toonOutline.y * max(abs(centerDepth), 1e-3);
            edge = max(edge, smoothstep(depthThreshold, depthThreshold * 2.0 + 1e-7, depthDifference));
            if (cwOutline.resolutionAndFlags.z != 0u && neighborDepth > 0.0)
            {
                vec3 neighborNormal = decodeOctahedral(texelFetch(cwNormalRoughnessMetallic, neighbor, 0).xy);
                float normalDifference = 1.0 - clamp(dot(centerNormal, neighborNormal), 0.0, 1.0);
                edge = max(edge, smoothstep(material.toonOutline.z,
                                             material.toonOutline.z + 0.1, normalDifference));
            }
        }
    }
    if (edge <= 0.0)
        return;

    float fadeDistance = material.toonOutline.w;
    float distanceFade = 1.0;
    if (fadeDistance > 0.0)
    {
        vec3 worldPosition = reconstructWorldPosition(pixel, centerDepth, dimensions);
        distanceFade = 1.0 - smoothstep(fadeDistance * 0.75, fadeDistance,
                                        length(cwOutline.cameraPosition.xyz - worldPosition));
    }
    vec4 current = imageLoad(cwHdrColor, pixel);
    float blend = clamp(edge * material.toonOutlineColor.a * distanceFade, 0.0, 1.0);
    imageStore(cwHdrColor, pixel, vec4(mix(current.rgb, material.toonOutlineColor.rgb, blend), current.a));
}

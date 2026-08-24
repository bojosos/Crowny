#lang glsl
#type compute
#version 450

#include "CrownyPbrLighting.glslinc"
#include "CrownyClusteredLighting.glslinc"

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(binding = 0) uniform ClusterConstants
{
    mat4 view;
    mat4 inverseProjection;
    uvec4 dimensionsAndLightCount;
    uvec4 viewportTileAndLimit;
    vec4 depthAndScale;
    uint visibilityLayerMask;
    uint maximumDirectionalLights;
    uint maximumLightIndices;
    uint padding;
} constants;

layout(std430, binding = 1) readonly buffer LightTable
{
    CwLightRecord lights[];
};

layout(std430, binding = 2) writeonly buffer ClusterCells
{
    CwClusterCell cells[];
};

layout(std430, binding = 3) writeonly buffer ClusterLightIndices
{
    uint lightIndices[];
};

layout(std430, binding = 4) writeonly buffer DirectionalLightIndices
{
    uint directionalIndices[];
};

layout(std430, binding = 5) buffer ClusterCounters
{
    uint lightIndexCount;
    uint directionalCount;
    uint overflowCount;
    uint padding;
} counters;

vec3 unprojectAtDepth(vec2 ndc, float viewDepth)
{
    vec4 point = constants.inverseProjection * vec4(ndc, 1.0, 1.0);
    vec3 ray = point.xyz / max(abs(point.w), 0.000001);
    return ray * (viewDepth / max(-ray.z, 0.000001));
}

void clusterBounds(uvec3 coordinate, out vec3 minimumBounds, out vec3 maximumBounds)
{
    float nearPlane = constants.depthAndScale.x;
    float farPlane = constants.depthAndScale.y;
    float sliceNear = nearPlane * pow(farPlane / nearPlane,
                                      float(coordinate.z) / float(constants.dimensionsAndLightCount.z));
    float sliceFar = nearPlane * pow(farPlane / nearPlane,
                                     float(coordinate.z + 1u) / float(constants.dimensionsAndLightCount.z));
    vec2 viewport = vec2(constants.viewportTileAndLimit.xy);
    float tileSize = float(constants.viewportTileAndLimit.z);
    vec2 pixelMinimum = vec2(coordinate.xy) * tileSize;
    vec2 pixelMaximum = min(pixelMinimum + tileSize, viewport);
    vec2 ndcMinimum = pixelMinimum / viewport * 2.0 - 1.0;
    vec2 ndcMaximum = pixelMaximum / viewport * 2.0 - 1.0;

    vec3 corners[8] = vec3[8](
        unprojectAtDepth(vec2(ndcMinimum.x, ndcMinimum.y), sliceNear),
        unprojectAtDepth(vec2(ndcMaximum.x, ndcMinimum.y), sliceNear),
        unprojectAtDepth(vec2(ndcMaximum.x, ndcMaximum.y), sliceNear),
        unprojectAtDepth(vec2(ndcMinimum.x, ndcMaximum.y), sliceNear),
        unprojectAtDepth(vec2(ndcMinimum.x, ndcMinimum.y), sliceFar),
        unprojectAtDepth(vec2(ndcMaximum.x, ndcMinimum.y), sliceFar),
        unprojectAtDepth(vec2(ndcMaximum.x, ndcMaximum.y), sliceFar),
        unprojectAtDepth(vec2(ndcMinimum.x, ndcMaximum.y), sliceFar));
    minimumBounds = corners[0];
    maximumBounds = corners[0];
    for (uint corner = 1u; corner < 8u; corner++)
    {
        minimumBounds = min(minimumBounds, corners[corner]);
        maximumBounds = max(maximumBounds, corners[corner]);
    }
}

bool sphereIntersectsBounds(vec3 center, float radius, vec3 minimumBounds, vec3 maximumBounds)
{
    vec3 closest = clamp(center, minimumBounds, maximumBounds);
    vec3 delta = center - closest;
    return dot(delta, delta) <= radius * radius;
}

void main()
{
    uvec3 dimensions = constants.dimensionsAndLightCount.xyz;
    uint clusterIndex = gl_GlobalInvocationID.x;
    uint clusterCount = dimensions.x * dimensions.y * dimensions.z;
    if (clusterIndex >= clusterCount)
        return;

    uvec3 coordinate;
    coordinate.x = clusterIndex % dimensions.x;
    coordinate.y = (clusterIndex / dimensions.x) % dimensions.y;
    coordinate.z = clusterIndex / (dimensions.x * dimensions.y);
    vec3 minimumBounds;
    vec3 maximumBounds;
    clusterBounds(coordinate, minimumBounds, maximumBounds);

    uint localIndices[128];
    uint localCount = 0u;
    uint maximumPerCluster = min(constants.viewportTileAndLimit.w, 128u);
    uint lightCount = constants.dimensionsAndLightCount.w;
    for (uint lightIndex = 0u; lightIndex < lightCount; lightIndex++)
    {
        CwLightRecord light = lights[lightIndex];
        uint lightType = light.metadata.x;
        uint lightFlags = light.metadata.y;
        if ((lightFlags & 1u) == 0u || (light.metadata.z & constants.visibilityLayerMask) == 0u)
            continue;

        if (lightType == 0u)
        {
            if (clusterIndex == 0u)
            {
                uint outputIndex = atomicAdd(counters.directionalCount, 1u);
                if (outputIndex < constants.maximumDirectionalLights)
                    directionalIndices[outputIndex] = lightIndex;
                else
                    atomicAdd(counters.overflowCount, 1u);
            }
            continue;
        }

        vec3 viewPosition = (constants.view * vec4(light.positionRange.xyz, 1.0)).xyz;
        if (!sphereIntersectsBounds(viewPosition, light.positionRange.w, minimumBounds, maximumBounds))
            continue;
        if (localCount < maximumPerCluster)
            localIndices[localCount++] = lightIndex;
        else
            atomicAdd(counters.overflowCount, 1u);
    }

    uint outputOffset = atomicAdd(counters.lightIndexCount, localCount);
    uint writableCount = outputOffset < constants.maximumLightIndices
                           ? min(localCount, constants.maximumLightIndices - outputOffset)
                           : 0u;
    cells[clusterIndex].offset = min(outputOffset, constants.maximumLightIndices);
    cells[clusterIndex].count = writableCount;
    for (uint index = 0u; index < writableCount; index++)
        lightIndices[outputOffset + index] = localIndices[index];
    if (writableCount != localCount)
        atomicAdd(counters.overflowCount, localCount - writableCount);
}

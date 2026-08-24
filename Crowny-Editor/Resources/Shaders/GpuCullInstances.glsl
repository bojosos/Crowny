#lang glsl
#type compute
#version 450

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

struct InstanceRecord
{
    vec4 currentTransform[3];
    vec4 previousTransform[3];
    vec4 boundingSphere;
    uvec4 draw;
};

struct MeshRecord
{
    uvec4 lodRangeAndHeaps;
    uvec4 geometryOffsets;
};

struct MeshLodRecord
{
    uint firstMeshlet;
    uint meshletCount;
    float error;
    uint padding;
};

struct VisibleInstanceRecord
{
    uint instanceId;
    uint lod;
};

layout(binding = 0) uniform CullingConstants
{
    vec4 frustumPlanes[6];
    mat4 view;
    mat4 projection;
    vec4 viewportAndNearPlane;
    uint instanceCapacity;
    uint maximumVisibleInstances;
    uint visibilityLayerMask;
    uint hiZMipCount;
    uint cameraCut;
    float occlusionBias;
    float maximumLodErrorPixels;
    uint padding0;
} culling;

layout(std430, binding = 1) readonly buffer InstanceTable
{
    InstanceRecord instances[];
};

layout(std430, binding = 2) writeonly buffer VisibleInstanceTable
{
    VisibleInstanceRecord visibleInstances[];
};

layout(std430, binding = 3) buffer VisibilityCounters
{
    uint visibleCount;
    uint overflowCount;
    uint hiddenCount;
    uint frustumCulledCount;
    uint occlusionCulledCount;
    uint padding0;
    uint padding1;
    uint padding2;
} counters;

layout(binding = 4) uniform sampler2D previousHiZ;
layout(std430, binding = 5) readonly buffer MeshTable
{
    MeshRecord meshes[];
};
layout(std430, binding = 6) readonly buffer MeshLodTable
{
    MeshLodRecord meshLods[];
};

bool sphereIntersectsFrustum(vec4 sphere)
{
    for (uint planeIndex = 0; planeIndex < 6; planeIndex++)
    {
        if (dot(culling.frustumPlanes[planeIndex], vec4(sphere.xyz, 1.0)) < -sphere.w)
            return false;
    }
    return true;
}

bool sphereOccluded(vec4 sphere)
{
    if (culling.cameraCut != 0u || culling.hiZMipCount == 0u)
        return false;
    vec3 viewPosition = (culling.view * vec4(sphere.xyz, 1.0)).xyz;
    float viewDepth = -viewPosition.z;
    float nearestViewDepth = max(viewDepth - sphere.w, culling.viewportAndNearPlane.z);
    if (viewDepth <= sphere.w || nearestViewDepth <= 0.0)
        return false;

    vec4 centerClip = culling.projection * vec4(viewPosition, 1.0);
    if (centerClip.w <= 0.0)
        return false;
    vec2 centerNdc = centerClip.xy / centerClip.w;
    vec2 radiusNdc = abs(vec2(culling.projection[0][0], culling.projection[1][1])) * sphere.w /
                     nearestViewDepth;
    vec2 minimumUv = clamp((centerNdc - radiusNdc) * 0.5 + 0.5, vec2(0.0), vec2(1.0));
    vec2 maximumUv = clamp((centerNdc + radiusNdc) * 0.5 + 0.5, vec2(0.0), vec2(1.0));
    vec2 rectanglePixels = (maximumUv - minimumUv) * culling.viewportAndNearPlane.xy;
    float largestDimension = max(max(rectanglePixels.x, rectanglePixels.y), 1.0);
    float mip = clamp(floor(log2(largestDimension)), 0.0, float(culling.hiZMipCount - 1u));

    float occluderDepth = 1.0;
    occluderDepth = min(occluderDepth, textureLod(previousHiZ, minimumUv, mip).r);
    occluderDepth = min(occluderDepth, textureLod(previousHiZ, vec2(maximumUv.x, minimumUv.y), mip).r);
    occluderDepth = min(occluderDepth, textureLod(previousHiZ, vec2(minimumUv.x, maximumUv.y), mip).r);
    occluderDepth = min(occluderDepth, textureLod(previousHiZ, maximumUv, mip).r);
    vec4 nearestClip = culling.projection * vec4(0.0, 0.0, -nearestViewDepth, 1.0);
    float nearestDeviceDepth = nearestClip.z / nearestClip.w;
    return nearestDeviceDepth <= occluderDepth - culling.occlusionBias;
}

uint selectLod(InstanceRecord instance)
{
    uint meshIndex = instance.draw.x & 0x00ffffffu;
    MeshRecord mesh = meshes[meshIndex];
    uint lodCount = mesh.lodRangeAndHeaps.y;
    if (lodCount <= 1u)
        return 0u;
    float viewDepth = max(-(culling.view * vec4(instance.boundingSphere.xyz, 1.0)).z, 1e-4);
    int packedBias = int(instance.draw.y) >> 24;
    float biasScale = exp2(clamp(float(packedBias) / 16.0, -8.0, 8.0));
    float projectionScale = abs(culling.projection[1][1]) * culling.viewportAndNearPlane.y * 0.5;
    uint selected = 0u;
    for (uint lod = 1u; lod < lodCount; lod++)
    {
        float projectedError = meshLods[mesh.lodRangeAndHeaps.x + lod].error * projectionScale /
                               max(viewDepth * biasScale, 1e-4);
        if (projectedError > culling.maximumLodErrorPixels)
            break;
        selected = lod;
    }
    return selected;
}

void main()
{
    uint instanceIndex = gl_GlobalInvocationID.x;
    if (instanceIndex >= culling.instanceCapacity)
        return;

    InstanceRecord instance = instances[instanceIndex];
    uint flags = instance.draw.x >> 24;
    uint meshHandle = instance.draw.x & 0x00ffffffu;
    if ((flags & 1u) == 0u || meshHandle == 0u || (instance.draw.z & culling.visibilityLayerMask) == 0u)
    {
        atomicAdd(counters.hiddenCount, 1u);
        return;
    }
    if (!sphereIntersectsFrustum(instance.boundingSphere))
    {
        atomicAdd(counters.frustumCulledCount, 1u);
        return;
    }
    if (sphereOccluded(instance.boundingSphere))
    {
        atomicAdd(counters.occlusionCulledCount, 1u);
        return;
    }

    uint outputIndex = atomicAdd(counters.visibleCount, 1u);
    if (outputIndex < culling.maximumVisibleInstances)
    {
        visibleInstances[outputIndex].instanceId = instanceIndex;
        visibleInstances[outputIndex].lod = selectLod(instance);
    }
    else
        atomicAdd(counters.overflowCount, 1u);
}

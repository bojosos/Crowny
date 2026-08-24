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

struct MeshletCandidate
{
    uint instanceId;
    uint meshletId;
};

layout(binding = 0) uniform ExpandConstants
{
    uint visibleInstanceCount;
    uint maximumCandidates;
    uint padding0;
    uint padding1;
} constants;
layout(std430, binding = 1) readonly buffer InstanceTable
{
    InstanceRecord instances[];
};
layout(std430, binding = 2) readonly buffer VisibleInstances
{
    VisibleInstanceRecord visibleInstances[];
};
layout(std430, binding = 3) readonly buffer MeshTable
{
    MeshRecord meshes[];
};
layout(std430, binding = 4) readonly buffer MeshLodTable
{
    MeshLodRecord meshLods[];
};
layout(std430, binding = 5) writeonly buffer MeshletCandidates
{
    MeshletCandidate candidates[];
};
layout(std430, binding = 6) buffer CandidateCounters
{
    uint candidateCount;
    uint overflowCount;
    uint padding0;
    uint padding1;
} counters;
layout(std430, binding = 7) readonly buffer VisibilityCounters
{
    uint visibleCount;
    uint visibilityOverflowCount;
    uint hiddenCount;
    uint frustumCulledCount;
    uint occlusionCulledCount;
    uint visibilityPadding0;
    uint visibilityPadding1;
    uint visibilityPadding2;
} visibility;

void main()
{
    uint visibleIndex = gl_GlobalInvocationID.x;
    uint visibleCount = min(visibility.visibleCount, constants.visibleInstanceCount);
    if (visibleIndex >= visibleCount)
        return;
    VisibleInstanceRecord visible = visibleInstances[visibleIndex];
    uint meshIndex = instances[visible.instanceId].draw.x & 0x00ffffffu;
    MeshRecord mesh = meshes[meshIndex];
    if (visible.lod >= mesh.lodRangeAndHeaps.y)
        return;
    MeshLodRecord lod = meshLods[mesh.lodRangeAndHeaps.x + visible.lod];
    uint outputOffset = atomicAdd(counters.candidateCount, lod.meshletCount);
    uint writableCount = outputOffset < constants.maximumCandidates
                           ? min(lod.meshletCount, constants.maximumCandidates - outputOffset)
                           : 0u;
    for (uint index = 0u; index < writableCount; index++)
    {
        candidates[outputOffset + index].instanceId = visible.instanceId;
        candidates[outputOffset + index].meshletId = lod.firstMeshlet + index;
    }
    if (writableCount != lod.meshletCount)
        atomicAdd(counters.overflowCount, lod.meshletCount - writableCount);
}

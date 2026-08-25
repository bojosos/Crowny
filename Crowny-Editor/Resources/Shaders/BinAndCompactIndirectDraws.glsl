#lang glsl
#type compute
#version 450

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

struct VisibleDrawInstance
{
    uint instanceId;
    uint materialIndex;
};

struct IndexedIndirectCommand
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};

struct MaterialRecord
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

struct DrawBinLookupEntry
{
    uvec4 key0;
    uvec4 value;
};

layout(binding = 0) uniform DrawBinCompactionConstants
{
    uint maximumInputCommandCount;
    uint lookupMask;
    uint lookupCapacity;
    uint binCount;
    uint materialCount;
    uint padding0;
    uint padding1;
    uint padding2;
} constants;
layout(std430, binding = 1) readonly buffer CulledDrawInstances
{
    VisibleDrawInstance culledInstances[];
};
layout(std430, binding = 2) readonly buffer CulledIndirectCommands
{
    IndexedIndirectCommand culledCommands[];
};
layout(std430, binding = 3) readonly buffer DrawSortKeys
{
    uvec4 sortKeys[];
};
layout(std430, binding = 4) buffer DrawCounters
{
    uint drawCount;
    uint drawOverflowCount;
    uint frustumCulledCount;
    uint coneCulledCount;
    uint occlusionCulledCount;
    uint binRejectedCount;
    uint binOverflowCount;
    uint padding4;
} counters;
layout(std430, binding = 5) readonly buffer MaterialTable
{
    MaterialRecord materials[];
};
layout(std430, binding = 6) readonly buffer DrawBinTable
{
    DrawBinLookupEntry drawBins[];
};
layout(std430, binding = 7) writeonly buffer VisibleDrawInstances
{
    VisibleDrawInstance visibleInstances[];
};
layout(std430, binding = 8) writeonly buffer IndirectCommands
{
    IndexedIndirectCommand commands[];
};
layout(std430, binding = 9) buffer IndirectDrawCounts
{
    uint counts[];
};

uint hashValue(uint hash, uint value)
{
    return (hash ^ value) * 16777619u;
}

uint hashKey(uvec4 key0, uint materialTemplate)
{
    uint hash = 2166136261u;
    hash = hashValue(hash, key0.x);
    hash = hashValue(hash, key0.y);
    hash = hashValue(hash, key0.z);
    hash = hashValue(hash, key0.w);
    hash = hashValue(hash, materialTemplate);
    return hash ^ (hash >> 16u);
}

uint findBin(uvec4 key0, uint materialTemplate)
{
    if (constants.lookupCapacity == 0u)
        return 0xffffffffu;
    uint lookupIndex = hashKey(key0, materialTemplate) & constants.lookupMask;
    for (uint probe = 0u; probe < constants.lookupCapacity; probe++)
    {
        DrawBinLookupEntry entry = drawBins[lookupIndex];
        if (entry.value.y == 0xffffffffu)
            return 0xffffffffu;
        if (all(equal(entry.key0, key0)) && entry.value.x == materialTemplate)
            return lookupIndex;
        lookupIndex = (lookupIndex + 1u) & constants.lookupMask;
    }
    return 0xffffffffu;
}

void main()
{
    uint sourceIndex = gl_GlobalInvocationID.x;
    uint inputCount = min(counters.drawCount, constants.maximumInputCommandCount);
    if (sourceIndex >= inputCount || constants.binCount == 0u)
        return;

    VisibleDrawInstance instance = culledInstances[sourceIndex];
    if (instance.materialIndex >= constants.materialCount)
    {
        atomicAdd(counters.binRejectedCount, 1u);
        return;
    }

    uint alpha = (materials[instance.materialIndex].textureIndices1.w >> 8u) & 0xffu;
    if (alpha > 1u)
        return;

    const uint opaquePhase = 2u;
    const uint standardPipeline = 0u;
    const uint standardMaterialTemplate = 0u;
    uvec4 key0 = uvec4(opaquePhase, alpha, standardPipeline, sortKeys[sourceIndex].x);
    uint lookupIndex = findBin(key0, standardMaterialTemplate);
    if (lookupIndex == 0xffffffffu)
    {
        atomicAdd(counters.binRejectedCount, 1u);
        return;
    }

    DrawBinLookupEntry bin = drawBins[lookupIndex];
    uint binIndex = bin.value.y;
    if (binIndex >= constants.binCount)
    {
        atomicAdd(counters.binRejectedCount, 1u);
        return;
    }
    uint localCommand = atomicAdd(counts[binIndex], 1u);
    if (localCommand >= bin.value.w)
    {
        atomicAdd(counts[constants.binCount + binIndex], 1u);
        atomicAdd(counters.binOverflowCount, 1u);
        return;
    }

    uint destinationIndex = bin.value.z + localCommand;
    visibleInstances[destinationIndex] = instance;
    IndexedIndirectCommand command = culledCommands[sourceIndex];
    command.firstInstance = destinationIndex;
    commands[destinationIndex] = command;
}

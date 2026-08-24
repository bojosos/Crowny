#lang glsl
#type compute
#version 450
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_ballot : require

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(binding = 0) uniform RadixConstants
{
    uint elementCount;
    uint groupSize;
    uint groupCount;
    uint passIndex;
} constants;
layout(std430, binding = 1) readonly buffer InputKeys
{
    uvec4 inputKeys[];
};
layout(std430, binding = 2) readonly buffer InputValues
{
    uint inputValues[];
};
layout(std430, binding = 3) readonly buffer GroupOffsets
{
    uint groupOffsets[];
};
layout(std430, binding = 4) writeonly buffer OutputKeys
{
    uvec4 outputKeys[];
};
layout(std430, binding = 5) writeonly buffer OutputValues
{
    uint outputValues[];
};

// A 256-thread workgroup contains at most 32 Vulkan subgroups. Per-subgroup
// histograms let every lane derive a stable rank without serializing the group.
shared uint subgroupHistograms[32][256];

uint radixDigit(uvec4 key)
{
    uint word = constants.passIndex < 4u ? key.x : key.y;
    return (word >> ((constants.passIndex & 3u) * 8u)) & 0xffu;
}

void main()
{
    uint lane = gl_LocalInvocationID.x;
    for (uint subgroup = 0u; subgroup < 32u; subgroup++)
        subgroupHistograms[subgroup][lane] = 0u;
    barrier();

    uint elementIndex = gl_WorkGroupID.x * constants.groupSize + lane;
    bool isActive = lane < constants.groupSize && elementIndex < constants.elementCount;
    uvec4 key = isActive ? inputKeys[elementIndex] : uvec4(0u);
    uint digit = radixDigit(key);
    uint subgroupRank = 0u;
    uint subgroupTotal = 0u;
    for (uint member = 0u; member < gl_SubgroupSize; member++)
    {
        uint memberDigit = subgroupBroadcast(isActive ? digit : 256u, member);
        if (memberDigit == digit)
        {
            subgroupTotal++;
            if (member < gl_SubgroupInvocationID)
                subgroupRank++;
        }
    }
    if (isActive && subgroupRank == 0u)
        subgroupHistograms[gl_SubgroupID][digit] = subgroupTotal;
    barrier();

    if (!isActive)
        return;
    uint groupRank = subgroupRank;
    for (uint subgroup = 0u; subgroup < gl_SubgroupID; subgroup++)
        groupRank += subgroupHistograms[subgroup][digit];
    uint outputIndex = groupOffsets[gl_WorkGroupID.x * 256u + digit] + groupRank;
    outputKeys[outputIndex] = key;
    outputValues[outputIndex] = inputValues[elementIndex];
}

#lang glsl
#type compute
#version 450

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
layout(std430, binding = 2) writeonly buffer GroupHistograms
{
    uint groupHistograms[];
};

shared uint histogram[256];

uint radixDigit(uvec4 key)
{
    uint word = constants.passIndex < 4u ? key.x : key.y;
    return (word >> ((constants.passIndex & 3u) * 8u)) & 0xffu;
}

void main()
{
    uint lane = gl_LocalInvocationID.x;
    histogram[lane] = 0u;
    barrier();

    uint elementIndex = gl_WorkGroupID.x * constants.groupSize + lane;
    if (lane < constants.groupSize && elementIndex < constants.elementCount)
        atomicAdd(histogram[radixDigit(inputKeys[elementIndex])], 1u);
    barrier();

    if (gl_WorkGroupID.x < constants.groupCount)
        groupHistograms[gl_WorkGroupID.x * 256u + lane] = histogram[lane];
}

#lang glsl
#type compute
#version 450

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(binding = 0) uniform RadixPrefixConstants
{
    uint groupCount;
    uint padding0;
    uint padding1;
    uint padding2;
} constants;
layout(std430, binding = 1) readonly buffer GroupHistograms
{
    uint groupHistograms[];
};
layout(std430, binding = 2) writeonly buffer GroupOffsets
{
    uint groupOffsets[];
};

shared uint digitTotals[256];
shared uint digitBases[256];

void main()
{
    uint digit = gl_LocalInvocationID.x;
    uint total = 0u;
    for (uint group = 0u; group < constants.groupCount; group++)
        total += groupHistograms[group * 256u + digit];
    digitTotals[digit] = total;
    barrier();

    if (digit == 0u)
    {
        uint base = 0u;
        for (uint index = 0u; index < 256u; index++)
        {
            digitBases[index] = base;
            base += digitTotals[index];
        }
    }
    barrier();

    uint offset = digitBases[digit];
    for (uint group = 0u; group < constants.groupCount; group++)
    {
        uint histogramIndex = group * 256u + digit;
        groupOffsets[histogramIndex] = offset;
        offset += groupHistograms[histogramIndex];
    }
}

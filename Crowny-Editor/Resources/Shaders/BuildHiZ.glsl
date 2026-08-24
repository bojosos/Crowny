#lang glsl
#type compute
#version 450

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0) uniform HiZConstants
{
    uvec2 destinationSize;
    uint sourceMip;
    uint copySource;
} constants;

layout(binding = 1) uniform sampler2D sourceDepth;
layout(r32f, binding = 2) writeonly uniform image2D destinationHiZ;

void main()
{
    ivec2 destination = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(destination, ivec2(constants.destinationSize))))
        return;

    ivec2 sourceSize = textureSize(sourceDepth, int(constants.sourceMip));
    if (constants.copySource != 0u)
    {
        ivec2 coordinate = min(destination, sourceSize - 1);
        imageStore(destinationHiZ, destination,
                   vec4(texelFetch(sourceDepth, coordinate, int(constants.sourceMip)).r));
        return;
    }

    ivec2 source = destination * 2;
    float farthestDepth = 1.0;
    for (int y = 0; y < 2; y++)
    {
        for (int x = 0; x < 2; x++)
        {
            ivec2 coordinate = min(source + ivec2(x, y), sourceSize - 1);
            // Reverse-Z stores the farthest conservative occluder as the
            // minimum depth. Background pixels therefore keep a tile visible.
            farthestDepth = min(farthestDepth, texelFetch(sourceDepth, coordinate, int(constants.sourceMip)).r);
        }
    }
    imageStore(destinationHiZ, destination, vec4(farthestDepth));
}

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

struct MeshletRecord
{
    vec4 boundingSphere;
    vec4 normalCone;
    uvec4 draw;
    uvec4 geometry;
};

struct MeshletCandidate
{
    uint instanceId;
    uint meshletId;
};

struct IndexedIndirectCommand
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};

layout(binding = 0) uniform MeshletCullingConstants
{
    vec4 frustumPlanes[6];
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    vec4 viewportAndNearPlane;
    uint candidateCount;
    uint maximumDrawCount;
    uint visibilityLayerMask;
    uint hiZMipCount;
    float occlusionBias;
    uint cameraCut;
    uint padding0;
    uint padding1;
} constants;
layout(std430, binding = 1) readonly buffer InstanceTable
{
    InstanceRecord instances[];
};
layout(std430, binding = 2) readonly buffer MeshletTable
{
    MeshletRecord meshlets[];
};
layout(std430, binding = 3) readonly buffer MeshletCandidates
{
    MeshletCandidate candidates[];
};
layout(binding = 4) uniform sampler2D currentHiZ;
layout(std430, binding = 5) writeonly buffer VisibleDrawInstanceIds
{
    uvec2 visibleDrawInstances[];
};
layout(std430, binding = 6) writeonly buffer IndirectCommands
{
    IndexedIndirectCommand commands[];
};
layout(std430, binding = 7) writeonly buffer DrawSortKeys
{
    uvec4 sortKeys[];
};
layout(std430, binding = 8) buffer DrawCounters
{
    uint drawCount;
    uint overflowCount;
    uint frustumCulledCount;
    uint coneCulledCount;
    uint occlusionCulledCount;
    uint padding0;
    uint padding1;
    uint padding2;
} counters;
layout(std430, binding = 9) readonly buffer CandidateCounters
{
    uint candidateCount;
    uint candidateOverflowCount;
    uint candidatePadding0;
    uint candidatePadding1;
} candidateCounters;

vec3 transformPoint(InstanceRecord instance, vec3 point)
{
    vec4 value = vec4(point, 1.0);
    return vec3(dot(instance.currentTransform[0], value), dot(instance.currentTransform[1], value),
                dot(instance.currentTransform[2], value));
}

mat3 linearTransform(InstanceRecord instance)
{
    return transpose(mat3(instance.currentTransform[0].xyz, instance.currentTransform[1].xyz,
                          instance.currentTransform[2].xyz));
}

vec4 worldSphere(InstanceRecord instance, vec4 localSphere)
{
    mat3 linear = linearTransform(instance);
    float maximumScale = sqrt(dot(linear[0], linear[0]) + dot(linear[1], linear[1]) + dot(linear[2], linear[2]));
    return vec4(transformPoint(instance, localSphere.xyz), localSphere.w * maximumScale);
}

bool sphereIntersectsFrustum(vec4 sphere)
{
    for (uint planeIndex = 0u; planeIndex < 6u; planeIndex++)
        if (dot(constants.frustumPlanes[planeIndex], vec4(sphere.xyz, 1.0)) < -sphere.w)
            return false;
    return true;
}

bool coneBackfacing(InstanceRecord instance, MeshletRecord meshlet, vec4 sphere)
{
    if (meshlet.normalCone.w >= 1.0)
        return false;
    vec3 axis = normalize(transpose(inverse(linearTransform(instance))) * meshlet.normalCone.xyz);
    vec3 toCenter = sphere.xyz - constants.cameraPosition.xyz;
    float distanceToCenter = length(toCenter);
    return dot(toCenter, axis) >= meshlet.normalCone.w * distanceToCenter + sphere.w;
}

bool sphereOccluded(vec4 sphere)
{
    if (constants.cameraCut != 0u || constants.hiZMipCount == 0u)
        return false;
    vec3 viewPosition = (constants.view * vec4(sphere.xyz, 1.0)).xyz;
    float viewDepth = -viewPosition.z;
    float nearestViewDepth = max(viewDepth - sphere.w, constants.viewportAndNearPlane.z);
    if (viewDepth <= sphere.w)
        return false;
    vec4 centerClip = constants.projection * vec4(viewPosition, 1.0);
    vec2 centerNdc = centerClip.xy / centerClip.w;
    vec2 radiusNdc = abs(vec2(constants.projection[0][0], constants.projection[1][1])) * sphere.w / nearestViewDepth;
    vec2 minimumUv = clamp((centerNdc - radiusNdc) * 0.5 + 0.5, vec2(0.0), vec2(1.0));
    vec2 maximumUv = clamp((centerNdc + radiusNdc) * 0.5 + 0.5, vec2(0.0), vec2(1.0));
    vec2 rectanglePixels = (maximumUv - minimumUv) * constants.viewportAndNearPlane.xy;
    float mip = clamp(floor(log2(max(max(rectanglePixels.x, rectanglePixels.y), 1.0))),
                      0.0, float(constants.hiZMipCount - 1u));
    float occluderDepth = min(min(textureLod(currentHiZ, minimumUv, mip).r,
                                  textureLod(currentHiZ, vec2(maximumUv.x, minimumUv.y), mip).r),
                              min(textureLod(currentHiZ, vec2(minimumUv.x, maximumUv.y), mip).r,
                                  textureLod(currentHiZ, maximumUv, mip).r));
    vec4 nearestClip = constants.projection * vec4(0.0, 0.0, -nearestViewDepth, 1.0);
    return nearestClip.z / nearestClip.w <= occluderDepth - constants.occlusionBias;
}

void main()
{
    uint candidateIndex = gl_GlobalInvocationID.x;
    uint candidateCount = min(candidateCounters.candidateCount, constants.candidateCount);
    if (candidateIndex >= candidateCount)
        return;
    MeshletCandidate candidate = candidates[candidateIndex];
    InstanceRecord instance = instances[candidate.instanceId];
    MeshletRecord meshlet = meshlets[candidate.meshletId];
    if ((instance.draw.z & constants.visibilityLayerMask) == 0u)
        return;
    vec4 sphere = worldSphere(instance, meshlet.boundingSphere);
    if (!sphereIntersectsFrustum(sphere))
    {
        atomicAdd(counters.frustumCulledCount, 1u);
        return;
    }
    if (coneBackfacing(instance, meshlet, sphere))
    {
        atomicAdd(counters.coneCulledCount, 1u);
        return;
    }
    if (sphereOccluded(sphere))
    {
        atomicAdd(counters.occlusionCulledCount, 1u);
        return;
    }

    uint outputIndex = atomicAdd(counters.drawCount, 1u);
    if (outputIndex >= constants.maximumDrawCount)
    {
        atomicAdd(counters.overflowCount, 1u);
        return;
    }
    uint materialIndex = (instance.draw.y & 0x00ffffffu) + meshlet.draw.z;
    visibleDrawInstances[outputIndex] = uvec2(candidate.instanceId, materialIndex);
    commands[outputIndex].indexCount = meshlet.draw.y;
    commands[outputIndex].instanceCount = 1u;
    commands[outputIndex].firstIndex = meshlet.draw.x;
    commands[outputIndex].vertexOffset = int(meshlet.geometry.x);
    commands[outputIndex].firstInstance = outputIndex;
    sortKeys[outputIndex] = uvec4(meshlet.geometry.z, materialIndex, candidate.meshletId, outputIndex);
}

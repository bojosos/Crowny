#lang glsl
#type compute
#version 450

// Two dispatches, separated by storage barriers. No append atomics: each
// survivor keeps its input position relative to every other survivor.
layout(local_size_x = 128) in;
layout(std140, binding = 0) uniform CullingData {
    vec4 Planes[6];
    uvec4 Range; // first entry, entry count, stage, reset view count
} culling;
struct SpriteInstance {
    vec4 Rows[3];
    vec4 PreviousRows[3];
    vec4 Color;
    vec4 UvRect;
    uvec4 Metadata;
};
layout(std430, binding = 1) readonly buffer Instances { SpriteInstance Values[]; } instances;
layout(std430, binding = 2) readonly buffer InputOrder { uvec2 Values[]; } inputOrder;
layout(std430, binding = 3) buffer Prefix { uvec2 Values[]; } prefix;
layout(std430, binding = 4) buffer GroupCounts { uint Values[]; } groupCounts;
layout(std430, binding = 5) writeonly buffer OutputOrder { uvec2 Values[]; } outputOrder;
layout(std430, binding = 6) writeonly buffer Arguments { uint Values[]; } arguments;
layout(std430, binding = 7) buffer VisibleCount { uint Value; } visibleCount;
shared uint ranks[128];
shared uint groupOffset;

void main() {
    uint lane = gl_LocalInvocationID.x;
    uint index = gl_GlobalInvocationID.x;
    uint group = gl_WorkGroupID.x;
    if (culling.Range.z == 0u) {
        if (index == 0u && culling.Range.w != 0u) visibleCount.Value = 0u;
        uint visible = 0u;
        if (index < culling.Range.y) {
            SpriteInstance sprite = instances.Values[inputOrder.Values[culling.Range.x + index].x];
            vec3 center = vec3(sprite.Rows[0].w, sprite.Rows[1].w, sprite.Rows[2].w);
            vec3 xAxis = vec3(sprite.Rows[0].x, sprite.Rows[1].x, sprite.Rows[2].x);
            vec3 yAxis = vec3(sprite.Rows[0].y, sprite.Rows[1].y, sprite.Rows[2].y);
            float radius = 0.5 * (length(xAxis) + length(yAxis));
            visible = 1u;
            for (uint plane = 0u; plane < 6u; ++plane)
                if (dot(culling.Planes[plane].xyz, center) + culling.Planes[plane].w < -radius)
                    visible = 0u;
        }
        ranks[lane] = visible;
        barrier();
        for (uint offset = 1u; offset < 128u; offset <<= 1u) {
            uint preceding = lane >= offset ? ranks[lane - offset] : 0u;
            barrier();
            ranks[lane] += preceding;
            barrier();
        }
        if (index < culling.Range.y) prefix.Values[index] = uvec2(ranks[lane], visible);
        if (lane == 127u) groupCounts.Values[group] = ranks[lane];
    } else {
        // At most 512 groups per retained 65,536-entry submission. Each group
        // sums preceding group counts once; no cross-workgroup spin waits.
        if (lane == 0u) {
            uint offset = 0u;
            for (uint previous = 0u; previous < group; ++previous) offset += groupCounts.Values[previous];
            groupOffset = offset;
            if (group + 1u == gl_NumWorkGroups.x) {
                uint total = offset + groupCounts.Values[group];
                arguments.Values[0] = 6u;
                arguments.Values[1] = total;
                arguments.Values[2] = 0u;
                arguments.Values[3] = 0u;
                arguments.Values[4] = 0u;
                // Diagnostics only. This counter never determines submission.
                visibleCount.Value += total;
            }
        }
        barrier();
        if (index < culling.Range.y && prefix.Values[index].y != 0u)
            outputOrder.Values[groupOffset + prefix.Values[index].x - 1u] = inputOrder.Values[culling.Range.x + index];
    }
}

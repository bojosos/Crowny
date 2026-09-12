#lang glsl
#type compute
#version 450
layout(local_size_x = 64) in;
layout(set = 0, binding = 0) uniform CwDecalBuild
{
    mat4 view;
    mat4 projection;
    vec4 depthViewport;
    uvec4 dimensions;
    uvec4 counts;
} config;
layout(std430, set = 0, binding = 1) readonly buffer Bounds { vec4 bounds[]; };
layout(std430, set = 0, binding = 2) buffer Ranges { uvec4 ranges[]; };
layout(std430, set = 0, binding = 3) buffer Cells { uvec2 cells[]; };
layout(std430, set = 0, binding = 4) buffer Indices { uint indices[]; };
layout(std430, set = 0, binding = 5) buffer Cursors { uint cursors[]; };
layout(std430, set = 0, binding = 6) buffer Counters { uint counters[]; };
shared uint sorted[64];

uint slice(float depth)
{
    return min(uint(log(max(depth, config.depthViewport.x) / config.depthViewport.x) /
                    log(config.depthViewport.y / config.depthViewport.x) * float(config.dimensions.z)), config.dimensions.z - 1u);
}
uvec2 tile(vec2 ndc)
{
    vec2 pixel = (clamp(ndc, -1.0, 1.0) * 0.5 + 0.5) * config.depthViewport.zw;
    return min(uvec2(pixel) / max(config.dimensions.w, 1u), config.dimensions.xy - 1u);
}
uint cellIndex(uvec3 p) { return p.x + config.dimensions.x * (p.y + config.dimensions.y * p.z); }
void main()
{
    uint id = gl_GlobalInvocationID.x;
    uint stage = config.counts.w;
    if (stage == 0u)
    {
        if (id < config.counts.y) cells[id] = uvec2(0);
        if (id < 4u) counters[id] = 0u;
    }
    else if (stage == 1u)
    {
        if (id >= config.counts.x) return;
        ranges[id * 2u] = uvec4(0);
        vec3 center = (config.view * vec4(bounds[id].xyz, 1)).xyz;
        float radius = bounds[id].w;
        if (-center.z + radius < config.depthViewport.x || -center.z - radius > config.depthViewport.y) return;
        vec2 low = vec2(-1), high = vec2(1);
        if (-center.z - radius > config.depthViewport.x)
        {
            low = vec2(3.402823e38); high = -low;
            for (uint corner = 0u; corner < 8u; ++corner)
            {
                vec3 p = center + radius * vec3((corner & 1u) != 0u ? 1 : -1, (corner & 2u) != 0u ? 1 : -1, (corner & 4u) != 0u ? 1 : -1);
                vec4 clip = config.projection * vec4(p, 1);
                low = min(low, clip.xy / clip.w); high = max(high, clip.xy / clip.w);
            }
        }
        if (low.x > 1.0 || low.y > 1.0 || high.x < -1.0 || high.y < -1.0) return;
        uvec3 first = uvec3(tile(low), slice(max(-center.z - radius, config.depthViewport.x)));
        uvec3 last = uvec3(tile(high), slice(min(-center.z + radius, config.depthViewport.y)));
        ranges[id * 2u] = uvec4(first, 1);
        ranges[id * 2u + 1u] = uvec4(last, 0);
        for (uint z = first.z; z <= last.z; ++z)
            for (uint y = first.y; y <= last.y; ++y)
                for (uint x = first.x; x <= last.x; ++x) atomicAdd(cells[cellIndex(uvec3(x,y,z))].y, 1u);
    }
    else if (stage == 2u)
    {
        if (id >= config.counts.y) return;
        uint count = cells[id].y;
        cursors[id] = 0u;
        if (count > 64u) { cells[id] = uvec2(0, 0xffffffffu); atomicAdd(counters[1], 1u); return; }
        uint offset = atomicAdd(counters[0], count);
        if (offset > config.counts.z || count > config.counts.z - offset)
        {
            cells[id] = uvec2(0, 0xffffffffu); atomicAdd(counters[1], 1u); return;
        }
        cells[id].x = offset;
    }
    else if (stage == 3u)
    {
        if (id >= config.counts.x || ranges[id * 2u].w == 0u) return;
        uvec3 first = ranges[id * 2u].xyz, last = ranges[id * 2u + 1u].xyz;
        for (uint z = first.z; z <= last.z; ++z)
            for (uint y = first.y; y <= last.y; ++y)
                for (uint x = first.x; x <= last.x; ++x)
                {
                    uint cell = cellIndex(uvec3(x,y,z));
                    if (cells[cell].y == 0xffffffffu) continue;
                    uint local = atomicAdd(cursors[cell], 1u);
                    indices[cells[cell].x + local] = id;
                }
    }
    else
    {
        uint cell = gl_WorkGroupID.x + gl_WorkGroupID.y * gl_NumWorkGroups.x;
        if (cell >= config.counts.y || cells[cell].y == 0u || cells[cell].y == 0xffffffffu) return;
        uint local = gl_LocalInvocationID.x;
        uint count = cells[cell].y, offset = cells[cell].x;
        sorted[local] = local < count ? indices[offset + local] : 0xffffffffu;
        barrier();
        for (uint size = 2u; size <= 64u; size *= 2u)
            for (uint stride = size / 2u; stride > 0u; stride /= 2u)
            {
                uint mine = sorted[local], other = sorted[local ^ stride];
                bool ascending = ((local & size) == 0u) == ((local & stride) == 0u);
                barrier();
                sorted[local] = ascending ? min(mine, other) : max(mine, other);
                barrier();
            }
        if (local < count) indices[offset + local] = sorted[local];
    }
}

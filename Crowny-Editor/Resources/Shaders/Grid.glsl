#lang glsl
#type vertex
#version 450

#pragma cull false

blend_state {
    enabled = true;
    color = { srca, srcia, add };
    alpha = { srca, srcia, add };
};

layout (location = 0) in vec3 inPos;

layout (binding = 0) uniform VP
{
    mat4 viewProjection;
    vec3 cameraPos;
} vp;

layout (location = 0) out vec3 worldPos;

void main()
{
    worldPos = inPos;
    gl_Position = vp.viewProjection * vec4(inPos, 1.0);
}

#type fragment
#version 450

layout (location = 0) in vec3 worldPos;

layout (location = 0) out vec4 outColor;
layout (location = 1) out int outEntity;

layout (binding = 0) uniform VP
{
    mat4 viewProjection;
    vec3 cameraPos;
} vp;

// Pristine grid — based on the "Best Darn Grid Shader Yet" technique by Ben Golus.
// Uses screen-space derivatives for anti-aliased grid lines that stay sharp at any distance.
float pristineGrid(vec2 uv, vec2 lineWidth)
{
    vec4 uvDDXY = vec4(dFdx(uv), dFdy(uv));
    vec2 uvDeriv = vec2(length(uvDDXY.xz), length(uvDDXY.yw));
    bvec2 invertLine = greaterThan(lineWidth, vec2(0.5));
    vec2 targetWidth = mix(lineWidth, 1.0 - lineWidth, vec2(invertLine));
    vec2 drawWidth = clamp(targetWidth, uvDeriv, vec2(0.5));
    vec2 lineAA = uvDeriv * 1.5;
    vec2 gridUV = abs(fract(uv) * 2.0 - 1.0);
    gridUV = mix(1.0 - gridUV, gridUV, vec2(invertLine));
    vec2 grid2 = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);
    grid2 *= clamp(targetWidth / drawWidth, 0.0, 1.0);
    grid2 = mix(grid2, targetWidth, clamp(uvDeriv * 2.0 - 1.0, 0.0, 1.0));
    grid2 = mix(grid2, 1.0 - grid2, vec2(invertLine));
    return mix(grid2.x, 1.0, grid2.y);
}

void main()
{
    outEntity = 0;

    vec2 coord = worldPos.xz;
    float dist = length(worldPos - vp.cameraPos);

    // Fine grid (1m spacing)
    float fineGrid = pristineGrid(coord, vec2(0.02));
    float fineFade = 1.0 - smoothstep(20.0, 60.0, dist);
    fineGrid *= fineFade;

    // Coarse grid (10m spacing)
    float coarseGrid = pristineGrid(coord * 0.1, vec2(0.02));
    float coarseFade = 1.0 - smoothstep(80.0, 200.0, dist);
    coarseGrid *= coarseFade;

    float grid = max(fineGrid, coarseGrid);

    // Axis highlights
    vec2 deriv = fwidth(coord);
    float axisWidth = max(deriv.x, 0.03);
    float xAxis = smoothstep(axisWidth, 0.0, abs(coord.y));
    float zAxis = smoothstep(axisWidth, 0.0, abs(coord.x));
    float axisFade = 1.0 - smoothstep(80.0, 200.0, dist);

    vec3 color = vec3(0.4);
    color = mix(color, vec3(0.15, 0.15, 0.85), zAxis * axisFade);
    color = mix(color, vec3(0.85, 0.15, 0.15), xAxis * axisFade);

    float axisAlpha = max(xAxis, zAxis) * axisFade;
    float alpha = max(grid * 0.4, axisAlpha * 0.8);

    if (alpha < 0.001)
        discard;

    outColor = vec4(color, alpha);
}

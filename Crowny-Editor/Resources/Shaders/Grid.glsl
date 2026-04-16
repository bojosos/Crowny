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

layout (binding = 0) uniform cw_VP
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

layout (binding = 1) uniform cw_VP
{
    mat4 viewProjection;
    vec3 cameraPos;
} vp;

layout (binding = 2) uniform cw_GridParams
{
    float fineSize;
    float coarseSize;
    float lineWidth;
    float opacity;
    int   showAxes;
} gridParams;

// Pristine grid — based on the "Best Darn Grid Shader Yet" technique by Ben Golus.
// Uses screen-space derivatives for anti-aliased grid lines that stay sharp at any distance.
float pristineGrid(vec2 uv, vec2 lw)
{
    vec4 uvDDXY = vec4(dFdx(uv), dFdy(uv));
    vec2 uvDeriv = vec2(length(uvDDXY.xz), length(uvDDXY.yw));
    bvec2 invertLine = greaterThan(lw, vec2(0.5));
    vec2 targetWidth = mix(lw, 1.0 - lw, vec2(invertLine));
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
    vec2 lw = vec2(gridParams.lineWidth);

    // Fine grid
    float fineGrid = pristineGrid(coord / gridParams.fineSize, lw);
    float fineFade = 1.0 - smoothstep(gridParams.fineSize * 20.0, gridParams.fineSize * 60.0, dist);
    fineGrid *= fineFade;

    // Coarse grid
    float coarseGrid = pristineGrid(coord / gridParams.coarseSize, lw);
    float coarseFade = 1.0 - smoothstep(gridParams.coarseSize * 8.0, gridParams.coarseSize * 20.0, dist);
    coarseGrid *= coarseFade;

    float grid = max(fineGrid, coarseGrid);

    // Axis highlights
    vec3 color = vec3(0.4);
    float axisAlpha = 0.0;

    if (gridParams.showAxes != 0)
    {
        vec2 deriv = fwidth(coord);
        float axisWidth = max(deriv.x, 0.03);
        float xAxis = smoothstep(axisWidth, 0.0, abs(coord.y));
        float zAxis = smoothstep(axisWidth, 0.0, abs(coord.x));
        float axisFade = 1.0 - smoothstep(80.0, 200.0, dist);
        color = mix(color, vec3(0.15, 0.15, 0.85), zAxis * axisFade);
        color = mix(color, vec3(0.85, 0.15, 0.15), xAxis * axisFade);
        axisAlpha = max(xAxis, zAxis) * axisFade;
    }

    float alpha = max(grid * gridParams.opacity, axisAlpha * 0.8);

    if (alpha < 0.001)
        discard;

    outColor = vec4(color, alpha);
}

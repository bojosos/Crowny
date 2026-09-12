#lang glsl
#pragma material_model unlit
#type vertex
#version 450

layout (location = 0) in vec3 cw_Position;
layout (location = 1) in vec3 cw_Normal;
layout (location = 2) in vec3 cw_Tangent;
layout (location = 3) in vec3 cw_Bitangent;
layout (location = 4) in vec2 cw_TexCoord0;
layout (location = 5) in vec4 cw_Color;

layout (binding = 0) uniform cw_MVP
{
    mat4 viewProjection;
    mat4 model;
} mvp;

layout(location = 0) out DATA
{
    vec2 uv;
    vec4 color;
} vs_out;
layout(location = 2) out vec3 decalWorldPosition;
layout(location = 3) out vec3 decalGeometricNormal;

void main()
{
    vec3 worldPos = vec3(mvp.model * vec4(cw_Position, 1.0));
    vs_out.uv = cw_TexCoord0;
    vs_out.color = cw_Color;
    decalWorldPosition = worldPos;
    decalGeometricNormal = normalize(transpose(inverse(mat3(mvp.model))) * cw_Normal);
    gl_Position = mvp.viewProjection * vec4(worldPos, 1.0);
}

#type fragment
#version 450

layout(location = 0) in DATA
{
    vec2 uv;
    vec4 color;
} fs_in;
layout(location = 2) in vec3 decalWorldPosition;
layout(location = 3) in vec3 decalGeometricNormal;
#define CW_DECAL_COMPATIBILITY
#include "CrownyDecals.glslinc"
#include "CrownyToneMapping.glslinc"
layout(set = 2, binding = 6) uniform cw_DecalDraw { uvec4 receiver; } cwDecalDraw;

layout (binding = 1) uniform UnlitParams {
    // @color @name("Tint") @default(1.0, 1.0, 1.0, 1.0)
    vec4 tint;
} params;

// @name("Albedo Map") @default(white)
layout (binding = 2) uniform sampler2D albedoMap;

layout (location = 0) out vec4 outColor;
layout (location = 1) out int outEntity;

void main()
{
    outEntity = int(cwDecalDraw.receiver.x);
    vec4 texColor = texture(albedoMap, fs_in.uv);
    outColor = texColor * params.tint * fs_in.color;
    float originalOpacity = outColor.a;
    vec3 normal = normalize(decalGeometricNormal), emission = vec3(0);
    float roughness = 0.5, metallic = 0.0, ao = 1.0;
    cwApplyDecals(cwDecalDraw.receiver.x, cwDecalDraw.receiver.y & 225u, decalWorldPosition, normal,
                  dFdx(decalWorldPosition), dFdy(decalWorldPosition), length(cwDecalGrid.cameraPosition.xyz - decalWorldPosition), 1.0,
                  outColor.rgb, normal, roughness, metallic, ao, emission, outColor.a);
    bool core = cwDecalCoatingCore(originalOpacity, outColor.a);
    if (cwDecalConstants.counts.w == 1u) { if (!core) discard; outColor.a = 1.0; }
    else if (cwDecalConstants.counts.w == 2u && core) discard;
    outColor.rgb = acesFitted(outColor.rgb + emission);
}

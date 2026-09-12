#lang glsl
#pragma material_model decal
#type vertex
#version 450
layout(location = 0) in vec3 position;
layout(location = 0) out vec2 uv;
void main() { uv = position.xy * 0.5 + 0.5; gl_Position = vec4(position, 1.0); }
#type fragment
#version 450
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
// @name("Color and coverage") @default(white)
layout(binding = 0) uniform sampler2D decalColorMap;
// @name("Normal") @default(normal)
layout(binding = 1) uniform sampler2D decalNormalMap;
// @name("AO / Roughness / Metallic") @default(white)
layout(binding = 2) uniform sampler2D decalSurfaceMap;
// @name("Emission") @default(white)
layout(binding = 3) uniform sampler2D decalEmissionMap;
// @name("Coverage mask") @default(white)
layout(binding = 4) uniform sampler2D decalMaskMap;
layout(binding = 5) uniform DecalParameters
{
    // @color @name("Color") @default(1.0, 1.0, 1.0, 1.0)
    vec4 decalColor;
    // @color @name("Emission") @default(0.0, 0.0, 0.0, 1.0)
    vec4 decalEmission;
    // @name("Channel strengths: color / normal / roughness / metallic") @default(1.0, 1.0, 1.0, 1.0)
    vec4 decalStrengths;
    // @name("Channel strengths: AO / emission / corrections / coating") @default(1.0, 1.0, 1.0, 1.0)
    vec4 decalStrengths2;
    // @color @name("Correction tint") @default(1.0, 1.0, 1.0, 1.0)
    vec4 decalCorrectionTint;
    // @name("Channels: color=1 normal=2 roughness=4 metal=8 AO=16 emission=32 corrections=64 coating=128") @default(1)
    int decalChannels;
    // @name("Color blend: replace=0 multiply=1 add=2") @default(0)
    int decalColorBlend;
    // @name("Roughness blend: replace=0 multiply=1 add=2") @default(0)
    int decalRoughnessBlend;
    // @name("Emission blend: replace=0 multiply=1 add=2") @default(2)
    int decalEmissionBlend;
    // @name("Replace receiver normal") @default(0)
    int decalReplaceNormal;
    // @name("Roughness") @range(0.0, 1.0) @default(0.5)
    float decalRoughness;
    // @name("Metallic") @range(0.0, 1.0) @default(0.0)
    float decalMetallic;
    // @name("Ambient occlusion") @range(0.0, 1.0) @default(1.0)
    float decalAO;
    // @name("Coating opacity") @range(0.0, 1.0) @default(1.0)
    float decalCoating;
    // @name("Exposure") @default(0.0)
    float decalExposure;
    // @name("Hue rotation") @range(-180.0, 180.0) @default(0.0)
    float decalHue;
    // @name("Saturation") @range(0.0, 2.0) @default(1.0)
    float decalSaturation;
    // @name("Contrast") @range(0.0, 2.0) @default(1.0)
    float decalContrast;
    // @name("Mask threshold") @range(0.0, 1.0) @default(0.0)
    float decalMaskThreshold;
    // @name("Mask softness") @range(0.0, 1.0) @default(0.0)
    float decalMaskSoftness;
} p;
// This preview program provides reflection for a decal material. Mesh routing rejects its domain.
void main()
{
    color = texture(decalColorMap, uv) * p.decalColor;
    color.a *= texture(decalMaskMap, uv).r;
    color.rgb += texture(decalEmissionMap, uv).rgb * p.decalEmission.rgb * p.decalEmission.a;
    color.rgb += 1e-20 * (texture(decalNormalMap, uv).rgb + texture(decalSurfaceMap, uv).rgb);
}

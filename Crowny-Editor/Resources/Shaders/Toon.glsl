#lang glsl
#pragma material_model toon
#pass 0
#type vertex
#version 450

#pragma cull front

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

layout (binding = 1) uniform OutlineParams
{
    // @color @name("Outline Color") @default(0.0, 0.0, 0.0, 1.0)
    vec4 outlineColor;
    // @range(0.0, 5.0) @name("Thickness") @default(1.0)
    float thickness;
    // @range(0.0, 5.0) @name("Inverted Hull Width") @default(1.0)
    float toonSilhouetteWidth;
    // @range(0.0, 0.1) @name("Depth Threshold") @default(0.002)
    float toonOutlineDepthThreshold;
    // @range(0.0, 1.0) @name("Normal Threshold") @default(0.2)
    float toonOutlineNormalThreshold;
    // @range(0.0, 1000.0) @name("Distance Fade") @default(100.0)
    float toonOutlineDistanceFade;
} outline;

void main()
{
    mat4 mvpMatrix = mvp.viewProjection * mvp.model;
    vec4 clipPos = mvpMatrix * vec4(cw_Position, 1.0);
    vec3 clipNormal = mat3(mvpMatrix) * cw_Normal;

    vec2 screenNormal = normalize(clipNormal.xy);
    clipPos.xy += screenNormal * outline.toonSilhouetteWidth * clipPos.w * 0.01;

    gl_Position = clipPos;
}

#type fragment
#version 450

#pragma cull front

layout (binding = 1) uniform OutlineParams
{
    // @color @name("Outline Color") @default(0.0, 0.0, 0.0, 1.0)
    vec4 outlineColor;
    // @range(0.0, 5.0) @name("Thickness") @default(1.0)
    float thickness;
    float toonSilhouetteWidth;
    float toonOutlineDepthThreshold;
    float toonOutlineNormalThreshold;
    float toonOutlineDistanceFade;
} outline;

layout (location = 0) out vec4 outColor;
layout (location = 1) out int outEntity;

void main()
{
    if (outline.toonSilhouetteWidth <= 0.0) discard;
    outEntity = 0;
    outColor = outline.outlineColor;
}

#pass 1
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
    vec3 worldPos;
    vec3 normal;
    vec2 uv;
    vec4 color;
} vs_out;

void main()
{
    vec3 worldPos = vec3(mvp.model * vec4(cw_Position, 1.0));
    vs_out.worldPos = worldPos;
    vs_out.normal = transpose(inverse(mat3(mvp.model))) * cw_Normal;
    vs_out.uv = cw_TexCoord0;
    vs_out.color = cw_Color;
    gl_Position = mvp.viewProjection * vec4(worldPos, 1.0);
}

#type fragment
#version 450

layout(location = 0) in DATA
{
    vec3 worldPos;
    vec3 normal;
    vec2 uv;
    vec4 color;
} fs_in;

layout (binding = 2) uniform cw_SceneParams {
    vec4 lightPositionRange[4];
    vec4 lightDirectionOuter[4];
    vec4 lightColorIntensity[4];
    vec4 lightSpotSourceBias[4];
    ivec4 lightMetadata[4];
    int lightCount;
    vec3 camPos;
    mat4 view;
    float useIBL;
} scene;

layout (binding = 3) uniform ToonParams {
    // @color @name("Tint") @default(1.0, 1.0, 1.0, 1.0)
    vec4 tint;
    // @range(1.0, 10.0) @name("Bands") @default(4.0)
    float bands;
    // @range(0.0, 1.0) @name("Specular Size") @default(0.5)
    float specularSize;
    // @range(0.0, 2.0) @name("Specular Smoothness") @default(1.0)
    float specularSmoothness;
    // @range(0.1, 10.0) @name("Rim Power") @default(4.0)
    float rimPower;
    // @range(0.0, 1.0) @name("Rim Threshold") @default(0.1)
    float rimThreshold;
    // @range(0.0, 1.0) @name("Shadow Brightness") @default(0.2)
    float shadowBrightness;
    // @name("Alpha Cutoff") @default(0.5)
    float alphaCutoff;
    float alphaMode;
} toon;

layout (binding = 5) uniform ToonStyleParams {
    // @color @name("Shadow Tint") @default(0.2, 0.22, 0.3, 1.0)
    vec4 toonShadowColor;
    // @color @name("Specular Color") @default(1.0, 1.0, 1.0, 1.0)
    vec4 toonSpecularColor;
    // @color @name("Rim Color") @default(1.0, 1.0, 1.0, 1.0)
    vec4 toonRimColor;
    // @range(0.0, 0.5) @name("Band Smoothness") @default(0.08)
    float toonBandSmoothness;
    // @range(0.0, 1.0) @name("Specular Threshold") @default(0.8)
    float toonSpecularThreshold;
    // @range(0.0, 0.5) @name("Specular Edge") @default(0.05)
    float toonSpecularSmoothness;
    // @range(0.0, 4.0) @name("Specular Strength") @default(0.5)
    float toonSpecularStrength;
    // @range(0.0, 0.5) @name("Rim Edge") @default(0.08)
    float toonRimSmoothness;
    // @range(0.0, 4.0) @name("Rim Strength") @default(0.5)
    float toonRimStrength;
    // @range(0.0, 1.0) @name("Rim Shadow Mask") @default(0.75)
    float toonRimShadowMask;
    // @range(0.0, 4.0) @name("Indirect Strength") @default(0.5)
    float toonIndirectStrength;
    // @range(0.001, 128.0) @name("Pattern Scale") @default(16.0)
    float toonPatternScale;
    // @range(0.0, 1.0) @name("Pattern Strength") @default(0.0)
    float toonPatternStrength;
    // @range(0.0, 0.5) @name("Pattern Edge") @default(0.1)
    float toonPatternSmoothness;
    // @range(0.0, 1000.0) @name("Pattern Distance Fade") @default(50.0)
    float toonPatternDistanceFade;
    // @name("Pattern Mapping") @default(0)
    int toonPatternMapping;
    // @range(0.0, 1.0) @name("Ramp Strength") @default(0.0)
    float toonRampStrength;
    // @range(-1.0, 1.0) @name("Ramp Offset") @default(0.0)
    float toonRampOffset;
    // @range(0.0, 1.0) @name("Matcap Strength") @default(0.0)
    float toonMatcapStrength;
    // @range(-3.14159, 3.14159) @name("Matcap Rotation") @default(0.0)
    float toonMatcapRotation;
} toonStyle;

// @name("Albedo Map") @default(white)
layout (binding = 4) uniform sampler2D albedoMap;
// @name("Hatching / Scratch Pattern") @default(white)
layout (binding = 6) uniform sampler2D toonPatternTexture;
// @name("Diffuse Ramp") @default(white)
layout (binding = 7) uniform sampler2D toonRampTexture;
// @name("Matcap") @default(white)
layout (binding = 8) uniform sampler2D toonMatcapTexture;
layout (binding = 9) uniform samplerCube cw_samplerIrradiance;

layout (location = 0) out vec4 outColor;
layout (location = 1) out int outEntity;

#define CW_DECAL_COMPATIBILITY
#include "CrownyDecals.glslinc"
#include "CrownyPbrLighting.glslinc"
#include "CrownyToneMapping.glslinc"
layout(set = 2, binding = 6) uniform cw_DecalDraw { uvec4 receiver; } cwDecalDraw;

float samplePattern(vec3 normal, float scale)
{
    float signal;
    if (toonStyle.toonPatternMapping == 1)
    {
        vec3 weights = abs(normal);
        weights /= max(weights.x + weights.y + weights.z, 1e-5);
        signal = dot(vec3(texture(toonPatternTexture, fs_in.worldPos.yz * scale).r,
                          texture(toonPatternTexture, fs_in.worldPos.xz * scale).r,
                          texture(toonPatternTexture, fs_in.worldPos.xy * scale).r), weights);
    }
    else if (toonStyle.toonPatternMapping == 2)
    {
        vec4 clip = cwDecalGrid.cw_DecalViewProjection * vec4(fs_in.worldPos, 1.0);
        signal = texture(toonPatternTexture, (clip.xy / clip.w * 0.5 + 0.5) * scale).r;
    }
    else
        signal = texture(toonPatternTexture, fs_in.uv * scale).r;
    float fadeDistance = max(toonStyle.toonPatternDistanceFade, 0.0);
    float fade = fadeDistance <= 0.0 ? 1.0 : 1.0 - smoothstep(fadeDistance * 0.75, fadeDistance, length(scene.camPos - fs_in.worldPos));
    return mix(0.5, signal, fade);
}

void main()
{
    outEntity = int(cwDecalDraw.receiver.x);
    vec3 positionDx = dFdx(fs_in.worldPos), positionDy = dFdy(fs_in.worldPos);

    vec3 N = normalize(fs_in.normal);
    vec3 V = normalize(scene.camPos - fs_in.worldPos);

    vec4 sampledColor = texture(albedoMap, fs_in.uv) * toon.tint * fs_in.color;
    if (toon.alphaMode == 1.0 && sampledColor.a < toon.alphaCutoff) discard;
    vec3 albedo = sampledColor.rgb;
    vec3 emission = vec3(0);
    float roughness = 0.5, metallic = 0.0, ao = 1.0, opacity = sampledColor.a;
    cwApplyDecals(cwDecalDraw.receiver.x, cwDecalDraw.receiver.y, fs_in.worldPos, N, positionDx, positionDy,
                  length(scene.camPos - fs_in.worldPos), 1.0, albedo, N, roughness, metallic, ao, emission, opacity);
    bool core = cwDecalCoatingCore(sampledColor.a, opacity);
    if (cwDecalConstants.counts.w == 1u) { if (!core) discard; opacity = 1.0; }
    else if (cwDecalConstants.counts.w == 2u && core) discard;

    CwPbrSurface surface = CwPbrSurface(fs_in.worldPos, N, V, max(albedo, vec3(0.0)), roughness, metallic, ao);
    vec4 shadowBands = vec4(max(toonStyle.toonShadowColor.rgb, vec3(0.0)), clamp(toon.bands, 2.0, 16.0));
    vec4 specular = vec4(max(toonStyle.toonSpecularColor.rgb, vec3(0.0)), clamp(toonStyle.toonSpecularThreshold, 0.0, 1.0));
    vec4 rim = vec4(max(toonStyle.toonRimColor.rgb, vec3(0.0)), clamp(toon.rimThreshold, 0.0, 1.0));
    vec4 controls = vec4(clamp(toonStyle.toonBandSmoothness, 0.0, 0.5), clamp(toonStyle.toonSpecularSmoothness, 0.0, 0.5),
                         max(toonStyle.toonSpecularStrength, 0.0), clamp(toonStyle.toonRimSmoothness, 0.0, 0.5));
    vec4 artistic = vec4(max(toon.rimPower, 0.01), max(toonStyle.toonRimStrength, 0.0),
                         clamp(toonStyle.toonRimShadowMask, 0.0, 1.0), max(toonStyle.toonIndirectStrength, 0.0));
    vec4 pattern = vec4(max(toonStyle.toonPatternScale, 0.001), clamp(toonStyle.toonPatternStrength, 0.0, 1.0),
                        clamp(toonStyle.toonPatternSmoothness, 0.0, 0.5), max(toonStyle.toonPatternDistanceFade, 0.0));
    float patternSignal = samplePattern(N, pattern.x);
    vec3 color = emission;
    for (int i = 0; i < scene.lightCount; ++i)
    {
        CwLightRecord light = CwLightRecord(scene.lightPositionRange[i], scene.lightDirectionOuter[i], scene.lightColorIntensity[i],
                                           scene.lightSpotSourceBias[i], uvec4(scene.lightMetadata[i]));
        float rampU = clamp(cwToonRampCoordinate(surface, light) + clamp(toonStyle.toonRampOffset, -1.0, 1.0), 0.0, 1.0);
        vec3 ramp = texture(toonRampTexture, vec2(rampU, 0.5)).rgb;
        color += cwEvaluateToonDirectLight(surface, light, 1.0, 1.0, shadowBands, specular, rim, controls, artistic, pattern,
                                           patternSignal, ramp, clamp(toonStyle.toonRampStrength, 0.0, 1.0));
    }
    if (scene.useIBL > 0.5)
        color += texture(cw_samplerIrradiance, N).rgb * surface.baseColor * ao * artistic.w;
    if (toonStyle.toonMatcapStrength > 0.0)
    {
        vec2 uv = normalize(mat3(scene.view) * N).xy;
        float cosine = cos(toonStyle.toonMatcapRotation), sine = sin(toonStyle.toonMatcapRotation);
        uv = mat2(cosine, -sine, sine, cosine) * uv;
        vec3 matcap = texture(toonMatcapTexture, uv * 0.5 + 0.5).rgb;
        color = mix(color, surface.baseColor * matcap + emission, clamp(toonStyle.toonMatcapStrength, 0.0, 1.0));
    }
    outColor = vec4(acesFitted(color), opacity);
}

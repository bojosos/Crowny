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
    clipPos.xy += screenNormal * outline.thickness * clipPos.w * 0.01;

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
    float toonOutlineDepthThreshold;
    float toonOutlineNormalThreshold;
    float toonOutlineDistanceFade;
} outline;

layout (location = 0) out vec4 outColor;
layout (location = 1) out int outEntity;

void main()
{
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
    vs_out.normal = mat3(mvp.model) * cw_Normal;
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
    vec4 lightDir;
    vec3 camPos;
    float gamma;
    float exposure;
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

layout (location = 0) out vec4 outColor;
layout (location = 1) out int outEntity;

vec3 Uncharted2Tonemap(vec3 x)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

void main()
{
    outEntity = 0;

    vec3 N = normalize(fs_in.normal);
    vec3 L = normalize(-scene.lightDir.xyz);
    vec3 V = normalize(scene.camPos - fs_in.worldPos);
    vec3 H = normalize(V + L);

    vec3 albedo = texture(albedoMap, fs_in.uv).rgb * toon.tint.rgb * fs_in.color.rgb;

    // Cel-shaded diffuse
    float NdotL = dot(N, L);
    float halfLambert = NdotL * 0.5 + 0.5;
    float bandStep = 1.0 / toon.bands;
    float shade = floor(halfLambert * toon.bands) * bandStep;
    shade = max(shade, toon.shadowBrightness);

    vec3 diffuse = albedo * shade;

    // Toon specular
    float NdotH = dot(N, H);
    float specIntensity = pow(max(NdotH, 0.0), toon.specularSmoothness * 128.0);
    float specular = smoothstep(toon.specularSize - 0.01, toon.specularSize + 0.01, specIntensity);

    // Rim lighting
    float NdotV = 1.0 - max(dot(N, V), 0.0);
    float rimIntensity = pow(NdotV, toon.rimPower);
    float rimMask = smoothstep(toon.rimThreshold - 0.01, toon.rimThreshold + 0.01, halfLambert);
    float rim = rimIntensity * rimMask;

    // Combine
    vec3 color = diffuse + vec3(specular) + vec3(rim) * albedo;

    // Tone mapping
    color = Uncharted2Tonemap(color * scene.exposure);
    color = color * (1.0 / Uncharted2Tonemap(vec3(11.2)));
    color = pow(color, vec3(1.0 / scene.gamma));

    outColor = vec4(color, 1.0);
}

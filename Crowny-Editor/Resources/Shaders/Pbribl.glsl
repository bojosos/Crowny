#lang glsl
#pragma material_model standard
#type vertex
#version 450

#pragma cull false

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
    vec3 tangent;
    vec3 bitangent;
    vec2 uv;
	vec4 color;
} vs_out;

void main()
{
    vec3 locPos = vec3(mvp.model * vec4(cw_Position, 1.0));
    mat3 normalMatrix = transpose(inverse(mat3(mvp.model)));
    vs_out.worldPos = locPos;
    vs_out.normal = normalMatrix * cw_Normal;
    vs_out.uv = cw_TexCoord0;
	vs_out.color = cw_Color;
    vs_out.tangent = mat3(mvp.model) * cw_Tangent.xyz;
    vs_out.bitangent = mat3(mvp.model) * cw_Bitangent.xyz;
    gl_Position =  mvp.viewProjection * vec4(locPos, 1.0);
}

#type fragment
#version 450

layout(location = 0) in DATA
{
    vec3 worldPos;
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
    vec2 uv;
	vec4 color;
} fs_in;

layout (binding = 2) uniform cw_UBOParams {
	vec4 lightPositionRange[4];
	vec4 lightDirectionOuter[4];
	vec4 lightColorIntensity[4];
	vec4 lightSpotSourceBias[4];
	ivec4 lightMetadata[4];
	int lightCount;
	float gamma;
	float exposure;
	vec3 camPos;
} uboParams;

layout (binding = 3) uniform samplerCube cw_samplerIrradiance;
layout (binding = 4) uniform sampler2D cw_samplerBRDFLUT;
layout (binding = 5) uniform samplerCube cw_prefilteredMap;

// @name("Albedo Map") @default(white)
layout (binding = 6) uniform sampler2D albedoMap;
// @name("Metallic Map") @default(white)
layout (binding = 7) uniform sampler2D metallicMap;
// @name("Roughness Map") @default(white)
layout (binding = 8) uniform sampler2D roughnessMap;
// @name("Normal Map") @default(white)
layout (binding = 9) uniform sampler2D normalMap;
// @name("AO Map") @default(white)
layout (binding = 10) uniform sampler2D aoMap;

// @name("Emission Map") @default(white)
layout (binding = 12) uniform sampler2D emissiveMap;

#ifdef CW_PROCEDURAL_BASE_COLOR
#include "CrownyProceduralTexture.glslinc"
#endif

layout (binding = 11) uniform Parameters {
    // @color @name("Emission") @default(0.0, 0.0, 0.0, 1.0)
    vec4 emissive;
    // @name("Emission Intensity") @default(1.0)
    float emissiveIntensity;
    // @name("Alpha Cutoff") @default(0.5)
    float alphaCutoff;
    float alphaMode;
    // @color @name("Albedo") @default(1.0, 1.0, 1.0, 1.0)
    vec4 albedo;
    // @range(0.0, 1.0) @name("Roughness") @default(0.5)
    float roughness;
    // @range(0.0, 1.0) @name("Metalness") @default(0.0)
    float metalness;
    float useIBL;
} parameters;

layout (location = 0) out vec4 outColor;
layout (location = 1) out int outEntity;

vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 prefilteredReflection(vec3 R, float roughness)
{
	const float MAX_REFLECTION_LOD = 9.0; // todo: param/const
	float lod = roughness * MAX_REFLECTION_LOD;
	float lodf = floor(lod);
	float lodc = ceil(lod);
	vec3 a = textureLod(cw_prefilteredMap, R, lodf).rgb;
	vec3 b = textureLod(cw_prefilteredMap, R, lodc).rgb;
	return mix(a, b, lod - lodf);
}

vec3 calculateNormal()
{
	vec3 tangentNormal = texture(normalMap, fs_in.uv).xyz * 2.0 - 1.0;

	vec3 N = normalize(fs_in.normal);
	vec3 T = normalize(fs_in.tangent);
	vec3 B = normalize(fs_in.bitangent);
	mat3 TBN = mat3(T, B, N);
	return normalize(TBN * tangentNormal);
}

#define CW_DECAL_COMPATIBILITY
#include "CrownyDecals.glslinc"
#include "CrownyPbrLighting.glslinc"
#include "CrownyToneMapping.glslinc"
layout(set = 2, binding = 6) uniform cw_DecalDraw { uvec4 receiver; } cwDecalDraw;

void main()
{
    vec3 positionDx = dFdx(fs_in.worldPos), positionDy = dFdy(fs_in.worldPos);
	outEntity = int(cwDecalDraw.receiver.x);
#ifdef CW_PROCEDURAL_BASE_COLOR
    vec4 baseSample = cwEvaluateBaseColor(fs_in.uv, fs_in.worldPos, fs_in.normal);
#else
    vec4 baseSample = texture(albedoMap, fs_in.uv);
#endif
    float alpha = baseSample.a * parameters.albedo.a * fs_in.color.a;
    if (parameters.alphaMode == 1.0 && alpha < parameters.alphaCutoff)
        discard;

	vec3 N = calculateNormal();
	vec3 V = normalize(uboParams.camPos - fs_in.worldPos);

	vec3 albedo = max(baseSample.rgb * parameters.albedo.rgb * fs_in.color.rgb, vec3(0.0));
	float metallic = clamp(texture(metallicMap, fs_in.uv).r * parameters.metalness, 0.0, 1.0);
	float roughness = clamp(texture(roughnessMap, fs_in.uv).r * parameters.roughness, 0.045, 1.0);
    float ao = texture(aoMap, fs_in.uv).r;
    vec3 emission = texture(emissiveMap, fs_in.uv).rgb * parameters.emissive.rgb * parameters.emissiveIntensity;
    float receiverOpacity = alpha;
    cwApplyDecals(cwDecalDraw.receiver.x, cwDecalDraw.receiver.y, fs_in.worldPos, normalize(fs_in.normal),
                  positionDx, positionDy, length(uboParams.camPos - fs_in.worldPos), 1.0,
                  albedo, N, roughness, metallic, ao, emission, alpha);
    vec3 R = reflect(-V, N);
    bool core = cwDecalCoatingCore(receiverOpacity, alpha);
    if (cwDecalConstants.counts.w == 1u)
    {
        if (!core) discard;
        alpha = 1.0;
    }
    else if (cwDecalConstants.counts.w == 2u && core) discard;

	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metallic);

	// Direct lighting. This compatibility shader supports four selected lights;
	// Forward+ and Deferred+ consume the complete persistent light table.
	vec3 Lo = vec3(0.0);
	CwPbrSurface surface = CwPbrSurface(fs_in.worldPos, N, V, albedo, roughness, metallic, ao);
	for(int i = 0; i < uboParams.lightCount; i++) {
		CwLightRecord light = CwLightRecord(uboParams.lightPositionRange[i], uboParams.lightDirectionOuter[i],
		    uboParams.lightColorIntensity[i], uboParams.lightSpotSourceBias[i], uvec4(uboParams.lightMetadata[i]));
		Lo += cwEvaluateDirectLight(surface, light, 1.0, 1.0);
	}

	vec3 ambient = vec3(0.0);
	if (parameters.useIBL > 0.5) {
		vec2 brdf = texture(cw_samplerBRDFLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
		vec3 reflection = prefilteredReflection(R, roughness).rgb;
		vec3 irradiance = texture(cw_samplerIrradiance, N).rgb;
		vec3 diffuse = irradiance * albedo;
		vec3 F = F_SchlickR(max(dot(N, V), 0.0), F0, roughness);
		vec3 specular = reflection * (F * brdf.x + brdf.y);
		vec3 kD = (1.0 - F) * (1.0 - metallic);
		ambient = (kD * diffuse + specular) * ao;
	}

	vec3 color = ambient + Lo + emission;

	// Match the default scene presentation in ToneMap.glsl. Surface colors and
	// the render target are linear; sRGB conversion belongs to presentation.
#ifndef CW_LINEAR_OUTPUT
	color = acesFitted(color);
#endif

	outColor = vec4(color, alpha);
}

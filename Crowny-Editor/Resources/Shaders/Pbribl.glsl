#lang glsl
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
    mat3 normalMatrix = mat3(mvp.model);
    vs_out.worldPos = locPos;
    vs_out.normal = normalMatrix * cw_Normal;
    vs_out.uv = cw_TexCoord0;
	vs_out.color = cw_Color;
    vs_out.tangent = normalMatrix * cw_Tangent.xyz;
    vs_out.bitangent = normalMatrix * cw_Bitangent.xyz;
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

layout (binding = 11) uniform Parameters {
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

#define PI 3.1415926535897932384626433832795

// From http://filmicgames.com/archives/75
vec3 Uncharted2Tonemap(vec3 x)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

// Normal Distribution function --------------------------------------
float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(PI * denom*denom);
}

// Geometric Shadowing function --------------------------------------
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}

// Fresnel function ----------------------------------------------------
vec3 F_Schlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
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

vec3 specularContribution(vec3 L, vec3 V, vec3 N, vec3 F0, float metallic, float roughness, vec3 albedo)
{
	// Precalculate vectors and dot products
	vec3 H = normalize (V + L);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);
	float dotVH = clamp(dot(V, H), 0.0, 1.0);

	vec3 color = vec3(0.0);

	if (dotNL > 0.0) {
		// D = Normal distribution (Distribution of the microfacets)
		float D = D_GGX(dotNH, roughness);
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
		// F = Fresnel factor (Reflectance depending on angle of incidence)
		vec3 F = F_Schlick(dotVH, F0);
		vec3 spec = D * F * G / (4.0 * dotNL * dotNV + 0.001);
		vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
		color += (kD * albedo / PI + spec) * dotNL;
	}

	return color;
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

void main()
{
	outEntity = 0;

	vec3 N = calculateNormal();
	vec3 V = normalize(uboParams.camPos - fs_in.worldPos);
	vec3 R = reflect(-V, N);

	vec3 albedo = pow(texture(albedoMap, fs_in.uv).rgb * parameters.albedo.rgb * fs_in.color.rgb, vec3(2.2));
	float metallic = texture(metallicMap, fs_in.uv).r * parameters.metalness;
	float roughness = texture(roughnessMap, fs_in.uv).r * parameters.roughness;

	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metallic);

	// Direct lighting. This compatibility shader supports four selected lights;
	// Forward+ and Deferred+ consume the complete persistent light table.
	vec3 Lo = vec3(0.0);
	for(int i = 0; i < uboParams.lightCount; i++) {
		int lightType = uboParams.lightMetadata[i].x;
		int lightFlags = uboParams.lightMetadata[i].y;
		if ((lightFlags & 1) == 0)
			continue;

		vec3 L;
		float attenuation = 1.0;
		if (lightType == 0) {
			L = normalize(-uboParams.lightDirectionOuter[i].xyz);
		} else {
			vec3 toLight = uboParams.lightPositionRange[i].xyz - fs_in.worldPos;
			float distanceSquared = max(dot(toLight, toLight), 0.0001);
			float distanceToLight = sqrt(distanceSquared);
			L = toLight / distanceToLight;
			float sourceRadius = uboParams.lightSpotSourceBias[i].y;
			attenuation = 1.0 / max(distanceSquared, sourceRadius * sourceRadius);
			float normalizedDistance = distanceToLight / max(uboParams.lightPositionRange[i].w, 0.0001);
			float rangeWindow = max(1.0 - pow(normalizedDistance, 4.0), 0.0);
			attenuation *= rangeWindow * rangeWindow;

			if (lightType == 2) {
				vec3 lightToSurface = -L;
				float coneCosine = dot(lightToSurface, normalize(uboParams.lightDirectionOuter[i].xyz));
				float outerCosine = uboParams.lightDirectionOuter[i].w;
				float innerCosine = uboParams.lightSpotSourceBias[i].x;
				attenuation *= smoothstep(outerCosine, innerCosine, coneCosine);
			}
		}

		vec3 incidentLight = uboParams.lightColorIntensity[i].rgb * uboParams.lightColorIntensity[i].w * attenuation;
		// Temporary exposure bridge for the old in-material tonemapper. The new
		// pipeline applies pre-exposure and camera exposure after HDR lighting.
		incidentLight *= 0.001;
		Lo += specularContribution(L, V, N, F0, metallic, roughness, albedo) * incidentLight;
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
		ambient = (kD * diffuse + specular) * texture(aoMap, fs_in.uv).rrr;
	}

	vec3 color = ambient + Lo;

	// Tone mapping
	color = Uncharted2Tonemap(color * uboParams.exposure);
	color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));
	// Gamma correction
	color = pow(color, vec3(1.0f / uboParams.gamma));

	outColor = vec4(color, 1.0);
}

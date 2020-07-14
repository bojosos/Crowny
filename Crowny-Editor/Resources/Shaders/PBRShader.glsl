#type vertex
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_Uv;
layout(location = 2) in vec3 a_Normal;

uniform mat4 u_ProjectionMatrix;
uniform mat4 u_ViewMatrix;
uniform mat4 u_ModelMatrix;

out DATA
{
	vec3 Position;
	vec3 Normal;
	vec2 Uv;
} vs_out;

void main()
{

	vs_out.Position = vec3(u_ModelMatrix * vec4(a_Position, 1.0));
	vs_out.Uv = a_Uv;
	vs_out.Normal = a_Normal;

	gl_Position = u_ProjectionMatrix * u_ViewMatrix * vec4(vs_out.Position, 1.0);
}

#type fragment
#version 330 core

layout (location = 0) out vec4 outColor;

in DATA
{
	vec3 Position;
	vec3 Normal;
	vec2 Uv;
} fs_in;

uniform sampler2D u_AlbedoMap;
uniform sampler2D u_NormalMap;
uniform sampler2D u_MetallicMap;
uniform sampler2D u_RoughnessMap;
uniform sampler2D u_aoMap;

uniform vec3 lightPositions[4];
uniform vec3 lightColors[4];

uniform vec3 camPos;

const float PI = 3.14159265359;

vec3 GetNormalFromMap()
{
	vec3 tangentNormal = texture(u_NormalMap, fs_in.Uv).xyz * 2.0 - 1.0;

	vec3 Q1 = dFdx(fs_in.Position);
	vec3 Q2 = dFdy(fs_in.Position);
	vec2 st1 = dFdx(fs_in.Uv);
	vec2 st2 = dFdy(fs_in.Uv);

	vec3 N = normalize(fs_in.Normal);
	vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
}

// NDF - makes the dots on balls (Trowbridge-Reitz GGX)
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH * NdotH;

	float nom = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return nom / denom;
}

//Geometry function (Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;

	float nom = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

// Fresnel equation
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

void main(void) 
{
	vec3 albedo = pow(texture(u_AlbedoMap, fs_in.Uv).rgb, vec3(2.2));
	float metallic = texture(u_MetallicMap, fs_in.Uv).r;
	float roughness = texture(u_MetallicMap, fs_in.Uv).r;
	float ao = texture(u_MetallicMap, fs_in.Uv).r;

	vec3 N = GetNormalFromMap();
	vec3 V = normalize(camPos - fs_in.Position);

	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metallic);

	vec3 Lo = vec3(0.0);

	for(int i=0;i < 4; i++)
	{
		vec3 L = normalize(lightPositions[i] - fs_in.Position);
		vec3 H = normalize(V + L);
		float distance = length(lightPositions[i] - fs_in.Position);
		float attenuation = 1.0 / (distance * distance);
		vec3 radiance = lightColors[i] * attenuation;

		float NDF = DistributionGGX(N, H, roughness);
		float G = GeometrySmith(N, V, L, roughness);
		vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

		vec3 nominator = NDF * G * F;
		float denominator = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
		vec3 specular = nominator / denominator;

		vec3 kS = F;
		vec3 kD = vec3(1.0) - kS;

		kD *= 1.0 - metallic;
		float NdotL = max(dot(N, L), 0.0);
		Lo += (kD * albedo / PI + specular) * radiance * NdotL;
	}

	vec3 ambient = vec3(0.03) * albedo * ao;
	vec3 color = ambient + Lo;

	color = color  / (color + vec3(1.0));
	color = pow(color, vec3(1.0 / 2.2));
	outColor = vec4(color, 1.0);
}


#type vertex
#version 460 core

layout (location = 0) in vec3 cw_Position;
layout (location = 1) in vec3 cw_Normal;
layout (location = 2) in vec3 cw_Tangent;
layout (location = 3) in vec3 cw_Bitangent;
layout (location = 4) in vec2 cw_TexCoord0;
layout (location = 5) in vec4 cw_Color;

layout (binding = 0) uniform MVP
{
    mat4 viewProjection;
    mat4 model;
} mvp;

layout(location = 0) out DATA
{
    vec3 normal;
} vs_out;

void main() {
	float a = length(cw_Tangent) + length(cw_Bitangent) + length(cw_TexCoord0) + length(cw_Color);
	vec3 locPos = vec3(mvp.model * vec4(cw_Position, 1.0));
	vs_out.normal = cw_Normal;
	gl_Position =  mvp.viewProjection * vec4(locPos, 1.0 - a/10000.0);
}

#type fragment
#version 460 core

layout(location = 0) in DATA
{
    vec3 normal;
} fs_in;

layout (location = 0) out vec4 outColor;

void main() {
	vec3 lightDir = normalize(vec3(0.0, -0.5, -0.5));
	float diff = max(dot(fs_in.normal, -lightDir), 0.0);

	vec3 defaultColor = vec3(0.8, 0.8, 0.8);
	outColor=vec4(defaultColor * diff, 1.0);
}
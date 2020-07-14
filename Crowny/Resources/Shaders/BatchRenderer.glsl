#type vertex

#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_UV;
layout(location = 2) in float a_Tid;
layout(location = 3) in vec4 a_Color;

uniform mat4 u_ProjectionMatrix;
uniform mat4 u_ViewMatrix;
uniform mat4 u_ModelMatrix;

out DATA
{
	vec4 position;
	vec2 uv;
	float tid;
	vec4 color;
} vs_out;

void main()
{
	gl_Position = u_ProjectionMatrix * u_ViewMatrix * u_ModelMatrix * vec4(a_Position, 1.0);

	vs_out.position = u_ModelMatrix * vec4(a_Position, 1.0);
	vs_out.uv = a_UV;
	vs_out.tid = a_Tid;
	vs_out.color = a_Color;
}

#type fragment
#version 330 core

layout (location = 0) out vec4 color;

in DATA
{
	vec4 position;
	vec2 uv;
	float tid;
	vec4 color;
} fs_in;

uniform sampler2D u_Textures[32];

void main(void) {
	color = vec4(1.0, 1.0, 1.0, 0.5);
}


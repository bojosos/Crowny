#type vertex

#version 460 core

layout(location = 0) in vec4 a_Position;
layout(location = 1) in vec2 a_UV;
layout(location = 2) in float a_Tid;
layout(location = 3) in vec4 a_Color;

uniform mat4 cw_ProjectionMatrix;
uniform mat4 cw_ViewMatrix;

out DATA
{
	vec4 position;
	vec2 uv;
	float tid;
	float mid;
	vec4 color;
} vs_out;

void main()
{
	gl_Position = cw_ProjectionMatrix * cw_ViewMatrix * a_Position;
	//gl_Position = a_Position;
	vs_out.position = a_Position;
	vs_out.uv = a_UV;
	vs_out.tid = a_Tid;
	vs_out.mid = 0;
	vs_out.color = a_Color;
}

#type fragment
#version 460 core

layout (location = 0) out vec4 color;

in DATA
{
	vec4 position;
	vec2 uv;
	float tid;
	float mid;
	vec4 color;
} fs_in;

uniform sampler2D u_Textures[32];

void main(void) 
{
	vec4 texColor = fs_in.color * texture(u_Textures[int(fs_in.tid)], fs_in.uv);
	color = texColor;
}


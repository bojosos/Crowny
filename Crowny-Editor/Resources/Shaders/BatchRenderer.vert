#version 460 core

layout(location = 0) in vec4 a_Position;
layout(location = 1) in vec2 a_UV;
layout(location = 2) in float a_Tid;
layout(location = 3) in vec4 a_Color;

layout(binding = 0) uniform VP
{
  mat4 proj;
  mat4 view;
} vp;

layout(location = 0) out DATA
{
	vec4 position;
	vec2 uv;
	flat float tid;
	vec4 color;
} vs_out;

void main()
{
	gl_Position = vp.proj * vp.view * a_Position;
	// gl_Position = a_Position;
	vs_out.position = a_Position;
	vs_out.uv = a_UV;
	vs_out.tid = a_Tid;
	vs_out.color = a_Color;
}
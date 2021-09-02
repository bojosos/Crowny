#version 460 core

layout(location = 0) in vec4 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_UV;
layout(location = 3) in float a_Tid;
layout(location = 4) in int a_ObjectId;

layout(binding = 0) uniform VP
{
  mat4 proj;
  mat4 view;
} vp;

layout(location = 0) out DATA
{
	vec4 position;
	vec4 color;
	vec2 uv;
	flat float tid;
	flat int objectId;
} vs_out;

void main()
{
	gl_Position = vp.proj * vp.view * a_Position;
	// gl_Position = a_Position;
	vs_out.position = a_Position;
	vs_out.uv = a_UV;
	vs_out.tid = a_Tid;
	vs_out.color = a_Color;
	vs_out.objectId = a_ObjectId;
}
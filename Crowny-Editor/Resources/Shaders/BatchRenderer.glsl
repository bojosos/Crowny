#type vertex

#version 460 core

layout(location = 0) in vec4 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_UV;
layout(location = 3) in float a_Tid;
layout(location = 4) in int a_ObjectId;

layout(binding = 0) uniform VP
{
  mat4 view;
  mat4 proj;
} vp;

layout(location = 0) out DATA
{
	vec4 color;
	vec2 uv;
	flat float tid;
	flat int objectId;
} vs_out;

void main()
{
	gl_Position = vp.proj * vp.view * a_Position;
	vs_out.uv = a_UV;
	vs_out.tid = a_Tid;
	vs_out.color = a_Color;
	vs_out.objectId = a_ObjectId;
}

#type fragment
#version 460

#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in DATA
{
	vec4 color;
	vec2 uv;
	flat float tid;
    flat int objectId;
} fs_in;

layout(location = 0) out vec4 color1;
layout(location = 1) out int color2;

layout(binding = 1) uniform sampler2D u_Texture1; // white
layout(binding = 2) uniform sampler2D u_Texture2;
layout(binding = 3) uniform sampler2D u_Texture3;
layout(binding = 4) uniform sampler2D u_Texture4;
layout(binding = 5) uniform sampler2D u_Texture5;
layout(binding = 6) uniform sampler2D u_Texture6;
layout(binding = 7) uniform sampler2D u_Texture7;
layout(binding = 8) uniform sampler2D u_Texture8;


void main() {

    vec4 texColor;
    switch (int(fs_in.tid))
    {
    case 0: texColor = fs_in.color * texture(u_Texture1, fs_in.uv); break;
    case 1: texColor = fs_in.color * texture(u_Texture2, fs_in.uv); break;
    case 2: texColor = fs_in.color * texture(u_Texture3, fs_in.uv); break;
    case 3: texColor = fs_in.color * texture(u_Texture4, fs_in.uv); break;
    case 4: texColor = fs_in.color * texture(u_Texture5, fs_in.uv); break;
    case 5: texColor = fs_in.color * texture(u_Texture6, fs_in.uv); break;
    case 6: texColor = fs_in.color * texture(u_Texture7, fs_in.uv); break;
    case 7: texColor = fs_in.color * texture(u_Texture8, fs_in.uv); break;
    }
    
    color1 = texColor;
    color2 = int(fs_in.objectId);
}
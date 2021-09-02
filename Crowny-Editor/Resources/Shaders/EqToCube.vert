#version 460 core

layout (location = 0) in vec3 aPos;

layout (location = 0) out vec3 WorldPos;

layout (binding = 0) uniform VP
{
  mat4 proj;
  mat4 view;
} vp;


void main()
{
    WorldPos = aPos;  
    gl_Position =  vp.proj * vp.view * vec4(WorldPos, 1.0);
}


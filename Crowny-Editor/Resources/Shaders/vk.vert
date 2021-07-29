#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Color;
layout(location = 2) in vec2 a_TexCoord;
/*
layout(binding = 0) uniform MVP
{
  mat4 model;
  mat4 proj;
  mat4 view;
} mvp;
*/
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 TextureCoords;

void main() {
	fragColor = a_Color;
  TextureCoords = a_TexCoord;
  gl_Position = /*mvp.proj * mvp.view * mvp.model **/ vec4(a_Position.x, a_Position.y, 0, 1.0);
}

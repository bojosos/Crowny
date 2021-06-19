#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Color;

layout(binding = 0) uniform MVP
{
  mat4 model;
  mat4 view;
  mat4 proj;
} mvp;

layout(location = 0) out vec3 fragColor;

void main() {
	fragColor = colors[gl_VertexIndex % 3];
  gl_Position = mpv.proj * mvp.view * mvp.model * vec4(a_Position, 1.0);
}

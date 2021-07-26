#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Color;

layout(binding = 0) uniform MVP
{
  mat4 model;
  mat4 proj;
  mat4 view;
} mvp;

layout(location = 0) out vec3 fragColor;

void main() {
	fragColor = a_Color;
  gl_Position = mvp.proj * mvp.view * mvp.model * vec4(a_Position, 1.0);
}

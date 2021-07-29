#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 TextureCoords;

layout(location = 0) out vec4 color;

layout(binding = 0) uniform sampler2D texSampler;

void main() {
  color = texture(texSampler, TextureCoords);// * vec4(fragColor, 1.0);
}

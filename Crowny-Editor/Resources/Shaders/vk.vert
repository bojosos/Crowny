#version 450

layout(location = 0) in vec3 a_Position;

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

layout(location = 0) out vec3 fragColor;

void main() {
	fragColor = colors[gl_VertexIndex];
  gl_Position = vec4(a_Position, 1.0);
}

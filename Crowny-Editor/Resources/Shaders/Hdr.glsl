#type vertex
#version 430 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 textureCoordinates;

out vec2 TextureCoordinates;

void main() {
	gl_Position = vec4(position, 1.0);
	TextureCoordinates = textureCoordinates;
}

#type fragment
#version 430 core

in vec2 TextureCoordinates;

// Uniforms
// Texture for the hdr buffer
uniform sampler2D hdrBuffer;

// Controls exposure level of image
uniform float exposure;

out vec4 fragColor;

// Uses Reinhard tonemapping https://www.cs.utah.edu/~reinhard/cdrom/tonemap.pdf
// with an added controllable exposure component

void main() {
	vec3 color = texture(hdrBuffer, TextureCoordinates).rgb;
	vec3 result = vec3(1.0) - exp(-color * exposure);

	// Minor gamma correction. Need to expand on it
	const float gamma = 2.2;
	result = pow(result, vec3(1.0 / gamma));
	fragColor = vec4(result, 1.0);
}
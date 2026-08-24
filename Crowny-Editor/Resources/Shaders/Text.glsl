#lang glsl
#type vertex
#version 450 core

#pragma cull false

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoords;
layout(location = 3) in vec4 a_OutlineColor;
layout(location = 4) in float a_OutlineThickness;
layout(location = 5) in float a_Weight;
layout(location = 6) in vec2 a_LocalPosition;
layout(location = 7) in vec4 a_ClipRect;
layout(location = 8) in int a_Flags;
layout(location = 9) in int a_ObjectId;

layout(std140, binding = 0) uniform cw_Camera
{
	mat4 u_ViewProjection;
} camera;

struct VertexOutput
{
	vec4 Color;
	vec4 OutlineColor;
	vec2 TexCoord;
	float OutlineThickness;
	float Weight;
	vec2 LocalPosition;
	vec4 ClipRect;
};

layout (location = 0) out VertexOutput Output;
layout (location = 7) out flat int v_Flags;
layout (location = 8) out flat int v_EntityID;

void main()
{
	Output.Color = a_Color;
	Output.TexCoord = a_TexCoords;
	Output.OutlineColor = a_OutlineColor;
	Output.OutlineThickness = a_OutlineThickness;
	Output.Weight = a_Weight;
	Output.LocalPosition = a_LocalPosition;
	Output.ClipRect = a_ClipRect;

	v_Flags = a_Flags;
	v_EntityID = a_ObjectId;

	gl_Position = camera.u_ViewProjection * vec4(a_Position, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;

struct VertexOutput
{
	vec4 Color;
	vec4 OutlineColor;
	vec2 TexCoord;
	float OutlineThickness;
	float Weight;
	vec2 LocalPosition;
	vec4 ClipRect;
};

layout (location = 0) in VertexOutput Input;
layout (location = 7) in flat int v_Flags;
layout (location = 8) in flat int v_EntityID;

layout (binding = 1) uniform sampler2D u_FontAtlas;

float screenPxRange() {
	const float pxRange = 2.0; // set to distance field's pixel range
    vec2 unitRange = vec2(pxRange)/vec2(textureSize(u_FontAtlas, 0));
    vec2 screenTexSize = vec2(1.0)/fwidth(Input.TexCoord);
    return max(0.5*dot(unitRange, screenTexSize), 1.0);
}

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main()
{
	o_EntityID = v_EntityID;
	if ((v_Flags & 2) != 0 &&
		(Input.LocalPosition.x < Input.ClipRect.x || Input.LocalPosition.y < Input.ClipRect.y ||
		 Input.LocalPosition.x > Input.ClipRect.z || Input.LocalPosition.y > Input.ClipRect.w))
		discard;

	if ((v_Flags & 1) != 0)
	{
		o_Color = Input.Color;
		if (o_Color.a <= 0.0)
			discard;
		return;
	}

	vec3 msd = texture(u_FontAtlas, Input.TexCoord).rgb;
    float sd = median(msd.r, msd.g, msd.b);
	float screenPxDistance = screenPxRange()*(sd - 0.5 + Input.Weight);
    float fillOpacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);
    float outlineOpacity = clamp(screenPxDistance + max(Input.OutlineThickness, 0.0) + 0.5, 0.0, 1.0);
    o_Color = mix(Input.OutlineColor, Input.Color, fillOpacity);
    o_Color.a *= outlineOpacity;
	if (o_Color.a <= 0.0)
		discard;
}

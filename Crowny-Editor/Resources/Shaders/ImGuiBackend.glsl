#type vertex
#version 450 core
layout(location = 0) in vec2 cw_Position;
layout(location = 1) in vec2 cw_TexCoord0;
layout(location = 2) in vec4 cw_Color;

blend_state {
    enabled = true;
    color = { srcA, srcIA, add };
    alpha = { one, srcIA, add };
};

layout(std140, binding = 0) uniform uPushConstant {
    vec2 uScale;
    vec2 uTranslate;
} pc;

layout(location = 0) out struct {
    vec4 Color;
    vec2 UV;
} Out;

void main()
{
    Out.Color = cw_Color;
    Out.UV = cw_TexCoord0;
    gl_Position = vec4(cw_Position * pc.uScale + pc.uTranslate, 0, 1);
}

#type fragment
#version 450 core
layout(location = 0) out vec4 fColor;

layout(binding = 1) uniform sampler2D sTexture;

layout(location = 0) in struct {
    vec4 Color;
    vec2 UV;
} In;

void main()
{
    fColor = In.Color * texture(sTexture, In.UV.st);
}
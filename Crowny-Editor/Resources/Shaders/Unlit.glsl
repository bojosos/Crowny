#type vertex
#version 450

layout (location = 0) in vec3 cw_Position;
layout (location = 1) in vec3 cw_Normal;
layout (location = 2) in vec3 cw_Tangent;
layout (location = 3) in vec3 cw_Bitangent;
layout (location = 4) in vec2 cw_TexCoord0;
layout (location = 5) in vec4 cw_Color;

layout (binding = 0) uniform cw_MVP
{
    mat4 viewProjection;
    mat4 model;
} mvp;

layout(location = 0) out DATA
{
    vec2 uv;
    vec4 color;
} vs_out;

void main()
{
    vec3 worldPos = vec3(mvp.model * vec4(cw_Position, 1.0));
    vs_out.uv = cw_TexCoord0;
    vs_out.color = cw_Color;
    gl_Position = mvp.viewProjection * vec4(worldPos, 1.0);
}

#type fragment
#version 450

layout(location = 0) in DATA
{
    vec2 uv;
    vec4 color;
} fs_in;

layout (binding = 1) uniform UnlitParams {
    // @color @name("Tint") @default(1.0, 1.0, 1.0, 1.0)
    vec4 tint;
} params;

// @name("Albedo Map") @default(white)
layout (binding = 2) uniform sampler2D albedoMap;

layout (location = 0) out vec4 outColor;
layout (location = 1) out int outEntity;

void main()
{
    outEntity = 0;
    vec4 texColor = texture(albedoMap, fs_in.uv);
    outColor = texColor * params.tint * fs_in.color;
}

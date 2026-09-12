#lang glsl
#type vertex
#version 450 core
#pragma cull false
#pragma depth_write false

blend_state {
    enabled = true;
    color = { one, srcia, add };
    alpha = { one, srcia, add };
};

layout(std140, binding = 0) uniform DrawData {
    mat4 ViewProjection;
    uvec4 Draw;
} drawData;

struct SpriteInstance {
    vec4 Row0;
    vec4 Row1;
    vec4 Row2;
    vec4 PreviousRow0;
    vec4 PreviousRow1;
    vec4 PreviousRow2;
    vec4 Color;
    vec4 UvRect;
    uvec4 Metadata;
};
layout(std430, binding = 9) readonly buffer Instances { SpriteInstance Values[]; } instances;
layout(std430, binding = 10) readonly buffer Order { uvec2 Values[]; } orderData;
layout(location = 0) out vec2 v_Uv;
layout(location = 1) out vec4 v_Color;
layout(location = 2) flat out uint v_Texture;
layout(location = 3) flat out uint v_Object;

void main() {
    const vec2 corners[6] = vec2[6](vec2(0, 0), vec2(1, 0), vec2(1, 1),
                                   vec2(1, 1), vec2(0, 1), vec2(0, 0));
    uvec2 entry = orderData.Values[drawData.Draw.x + gl_InstanceIndex];
    SpriteInstance sprite = instances.Values[entry.x];
    vec2 corner = corners[gl_VertexIndex];
    vec4 local = vec4(corner - 0.5, 0, 1);
    gl_Position = drawData.ViewProjection * vec4(dot(sprite.Row0, local), dot(sprite.Row1, local), dot(sprite.Row2, local), 1);
    v_Uv = mix(sprite.UvRect.xy, sprite.UvRect.zw, corner);
    v_Color = sprite.Color;
    v_Texture = entry.y;
    v_Object = sprite.Metadata.x;
}

#type fragment
#version 450 core
layout(location = 0) in vec2 v_Uv;
layout(location = 1) in vec4 v_Color;
layout(location = 2) flat in uint v_Texture;
layout(location = 3) flat in uint v_Object;
layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_Object;
layout(binding = 1) uniform sampler2D Texture0;
layout(binding = 2) uniform sampler2D Texture1;
layout(binding = 3) uniform sampler2D Texture2;
layout(binding = 4) uniform sampler2D Texture3;
layout(binding = 5) uniform sampler2D Texture4;
layout(binding = 6) uniform sampler2D Texture5;
layout(binding = 7) uniform sampler2D Texture6;
layout(binding = 8) uniform sampler2D Texture7;

void main() {
    vec4 color = vec4(0);
    switch (v_Texture) {
    case 0: color = texture(Texture0, v_Uv); break;
    case 1: color = texture(Texture1, v_Uv); break;
    case 2: color = texture(Texture2, v_Uv); break;
    case 3: color = texture(Texture3, v_Uv); break;
    case 4: color = texture(Texture4, v_Uv); break;
    case 5: color = texture(Texture5, v_Uv); break;
    case 6: color = texture(Texture6, v_Uv); break;
    case 7: color = texture(Texture7, v_Uv); break;
    }
    color *= v_Color;
    if (color.a <= 0.0) discard;
    o_Color = vec4(color.rgb * color.a, color.a);
    o_Object = int(v_Object);
}

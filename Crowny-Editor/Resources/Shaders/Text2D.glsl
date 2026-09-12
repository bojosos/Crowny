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

layout(std140, binding = 0) uniform TextDraw {
    mat4 ViewProjection;
    mat4 World;
    vec4 Color;
    vec4 OutlineColor;
    vec4 ClipRect;
    vec4 Style;
    vec4 Decoration;
    vec4 Offset;
    ivec4 Metadata;
} drawData;

struct Glyph { vec4 Rect; vec4 UvRect; vec4 Shape; };
layout(std430, binding = 2) readonly buffer Glyphs { Glyph Values[]; } glyphs;
layout(location = 0) out vec2 v_Uv;
layout(location = 1) out vec2 v_Local;
layout(location = 2) flat out vec4 v_Color;
layout(location = 3) flat out vec4 v_OutlineColor;
layout(location = 4) flat out vec4 v_ClipRect;
layout(location = 5) flat out vec4 v_Shape;
layout(location = 6) flat out ivec4 v_Metadata;

void main() {
    const vec2 corners[6] = vec2[6](vec2(0, 0), vec2(1, 0), vec2(1, 1),
                                   vec2(1, 1), vec2(0, 1), vec2(0, 0));
    Glyph glyph = glyphs.Values[gl_InstanceIndex];
    vec2 corner = corners[gl_VertexIndex];
    vec2 local = mix(glyph.Rect.xy, glyph.Rect.zw, corner);
    int kind = int(glyph.Shape.z);
    v_Color = drawData.Color;
    if (kind != 0) {
        local.y += (corner.y - 0.5) * drawData.Decoration.x +
                    (kind == 1 ? drawData.Decoration.y : drawData.Decoration.z);
        if ((int(drawData.Decoration.w) & kind) == 0) v_Color.a = 0;
    } else {
        local.x += drawData.Style.x * (local.y - glyph.Shape.x);
        local += drawData.Offset.xy;
    }
    v_Local = local;
    v_Uv = mix(glyph.UvRect.xy, glyph.UvRect.zw, corner);
    v_OutlineColor = drawData.OutlineColor;
    v_ClipRect = drawData.ClipRect;
    v_Shape = vec4(glyph.Shape.y, drawData.Style.yzw);
    v_Metadata = drawData.Metadata;
    gl_Position = drawData.ViewProjection * drawData.World * vec4(local, 0, 1);
}

#type fragment
#version 450 core
layout(binding = 1) uniform sampler2D FontAtlas;
layout(location = 0) in vec2 v_Uv;
layout(location = 1) in vec2 v_Local;
layout(location = 2) flat in vec4 v_Color;
layout(location = 3) flat in vec4 v_OutlineColor;
layout(location = 4) flat in vec4 v_ClipRect;
layout(location = 5) flat in vec4 v_Shape;
layout(location = 6) flat in ivec4 v_Metadata;
layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_Object;

void main() {
    if (v_Metadata.y != 0 && (any(lessThan(v_Local, v_ClipRect.xy)) || any(greaterThan(v_Local, v_ClipRect.zw)))) discard;
    o_Object = v_Metadata.x;
    if (v_Metadata.z != 0) {
        o_Color = vec4(v_Color.rgb * v_Color.a, v_Color.a);
    } else {
        vec3 msd = texture(FontAtlas, v_Uv).rgb;
        float sd = max(min(msd.r, msd.g), min(max(msd.r, msd.g), msd.b));
        vec2 unitRange = vec2(max(v_Shape.x, 0.5)) / vec2(textureSize(FontAtlas, 0));
        float pxRange = max(0.5 * dot(unitRange, vec2(1) / fwidth(v_Uv)), 1.0);
        float distance = pxRange * (sd - 0.5 + v_Shape.y) / max(1.0 + v_Shape.w, 1.0);
        float fill = clamp(distance + 0.5, 0.0, 1.0);
        float outline = clamp(distance + max(v_Shape.z, 0.0) + 0.5, 0.0, 1.0);
        o_Color = mix(v_OutlineColor, v_Color, fill);
        o_Color.a *= outline;
        o_Color.rgb *= o_Color.a;
    }
    if (o_Color.a <= 0) discard;
}

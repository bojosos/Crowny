#include "cwpch.h"

#include "Crowny/Renderer/Renderer2D.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/RenderAPI/GraphicsPipeline.h"
#include "Crowny/RenderAPI/RenderCommand.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/UniformParams.h"
#include "Crowny/RenderAPI/VertexArray.h"
#include "Crowny/Renderer/Camera.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/FontManager.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/TextLayout.h"
#include "Crowny/Utils/ShaderCompiler.h"

#include "Crowny/Renderer/MSDFdata.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <tracy/Tracy.hpp>

#include <limits>

namespace Crowny
{
    using namespace Literals;

    const std::array<glm::vec2, 4> QuadUv = { { glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f) } };
    const std::array<glm::vec4, 4> QuadVertices = { { glm::vec4(-0.5f, -0.5f, 0.0f, 1.0f), glm::vec4(0.5f, -0.5f, 0.0f, 1.0f),
                                                      glm::vec4(0.5f, 0.5f, 0.0f, 1.0f), glm::vec4(-0.5f, 0.5f, 0.0f, 1.0f) } };

    struct VertexData
    {
        glm::vec4 Position;
        glm::vec4 Color;
        glm::vec2 Uv;
        float Tid; // Why float?
        int32_t ObjectID;
    };

    struct CircleVertex
    {
        glm::vec3 WorldPosition;
        glm::vec3 LocalPosition;
        glm::vec4 Color;
        float Thickness;
        float Fade;

        int32_t ObjectID;
    };

    struct TextVertex
    {
        glm::vec3 Position;
        glm::vec4 Color;
        glm::vec2 TexCoords;
        glm::vec4 OutlineColor;
        float OutlineThickness;
        float Weight;
        float PixelRange;
        float Softness;
        glm::vec2 LocalPosition;
        glm::vec4 ClipRect;
        int32_t Flags;
        int32_t ObjectId;
    };

    struct Renderer2DData
    {
        static const uint32_t MaxLines = 20000;
        static const uint32_t MaxLineVertices = MaxLines * 4;
        static const uint32_t MaxLineIndices = MaxLines * 6;
        static const uint32_t MaxTextGlyphs = RENDERER_MAX_SPRITES;
        static const uint32_t MaxTextVertices = MaxTextGlyphs * 4;

        // Quads
        Ref<VertexBuffer> QuadVertexBuffer;
        Ref<IndexBuffer> QuadIndexBuffer;
        uint32_t QuadIndexCount = 0;
        uint32_t QuadVertexCount = 0;
        VertexData* QuadBuffer = nullptr;
        VertexData* QuadTmpBuffer = nullptr;
        Ref<Material> QuadMaterial;

        // Circles
        Ref<VertexBuffer> CircleVertexBuffer;
        uint32_t CircleIndexCount = 0;
        uint32_t CircleVertexCount = 0;
        CircleVertex* CircleBuffer = nullptr;    // TODO: Better naming for these like base and current
        CircleVertex* CircleTmpBuffer = nullptr; // TODO: Better naming for these like base and current
        Ref<Material> CircleMaterial;

        // Text
        Ref<VertexBuffer> TextVertexBuffer;
        uint32_t TextIndexCount = 0;
        uint32_t TextVertexCount = 0;
        TextVertex* TextBuffer = nullptr;
        TextVertex* TextTmpBuffer = nullptr;
        Ref<Material> TextMaterial;
        TextLayoutScratch TextLayout;

        // Font atlas only texture
        Ref<Texture> FontAtlasTexture;

        // Global texture buffer
        std::array<Ref<Texture>, 32> Textures;
        std::array<String, 32> TextureUniformNames;
        std::array<HashedString, 32> TextureUniformHashes;
        uint32_t TextureIndex = 0;

        // Camera axes used to turn world-space debug lines into camera-facing quads.
        glm::vec3 CameraForward{ 0.0f, 0.0f, -1.0f };
        glm::vec3 CameraUp{ 0.0f, 1.0f, 0.0f };
    };

    static Renderer2DData* s_Data;
    static void FlushText();

    static void ResetTextBatch()
    {
        s_Data->TextBuffer = s_Data->TextTmpBuffer;
        s_Data->TextIndexCount = 0;
        s_Data->TextVertexCount = 0;
    }

    static void SetupQuadBuffers()
    {
        uint32_t* indices = new uint32_t[RENDERER_INDICES_SIZE];
        uint32_t offset = 0;
        for (int i = 0; i < RENDERER_INDICES_SIZE; i += 6)
        {
            indices[i + 0] = offset + 0;
            indices[i + 1] = offset + 1;
            indices[i + 2] = offset + 2;

            indices[i + 3] = offset + 2;
            indices[i + 4] = offset + 3;
            indices[i + 5] = offset + 0;

            offset += 4;
        }

        s_Data->QuadIndexBuffer = IndexBuffer::Create({ RENDERER_INDICES_SIZE, IndexType::Index_32, BufferUsage::BU_STATIC_DRAW, indices });
        s_Data->QuadVertexBuffer = VertexBuffer::Create({ RENDERER_BUFFER_SIZE, BufferUsage::BU_DYNAMIC_DRAW });
        const Ref<BufferLayout> layout =
          CreateRef<BufferLayout>(BufferLayout{ BufferElement(ShaderDataType::Float4, "a_Position"), BufferElement(ShaderDataType::Float4, "a_Color"),
                                                BufferElement(ShaderDataType::Float2, "a_Uvs"), BufferElement(ShaderDataType::Float, "a_Tid"),
                                                BufferElement(ShaderDataType::Int, "a_ObjectId") });
        s_Data->QuadVertexBuffer->SetLayout(layout);

        const AssetHandle<Shader> shaderHandle = AssetManager::TryGet()->Load<Shader>(RENDERER2D_SHADER_PATH);
        s_Data->QuadMaterial = Material::Create(shaderHandle);
        s_Data->QuadBuffer = s_Data->QuadTmpBuffer = new VertexData[RENDERER_MAX_SPRITES * 4];
        delete[] indices;
    }

    static void SetupCircleBuffers()
    {
        s_Data->CircleBuffer = s_Data->CircleTmpBuffer = new CircleVertex[s_Data->MaxLineVertices];
        s_Data->CircleVertexBuffer =
          VertexBuffer::Create({ static_cast<uint32_t>(s_Data->MaxLineVertices * sizeof(CircleVertex)), BufferUsage::BU_DYNAMIC_DRAW });
        const Ref<BufferLayout> layout = CreateRef<BufferLayout>(BufferLayout{ { ShaderDataType::Float3, "a_WorldPosition" },
                                                                               { ShaderDataType::Float3, "a_LocalPosition" },
                                                                               { ShaderDataType::Float4, "a_Color" },
                                                                               { ShaderDataType::Float, "a_Thickness" },
                                                                               { ShaderDataType::Float, "a_Fade" },
                                                                               { ShaderDataType::Int, "a_Id" } });
        s_Data->CircleVertexBuffer->SetLayout(layout);

        const AssetHandle<Shader> shaderHandle = AssetManager::TryGet()->Load<Shader>("Resources/Shaders/Circle.asset");
        // const Ref<Shader> circleShader = Importer::Get().Import<Shader>("Resources/Shaders/Circle.glsl");
        // AssetManager::TryGet()->Save(circleShader, "Resources/Shaders/Circle.asset");
        // const AssetHandle<Shader> shaderHandle = static_asset_cast<Shader>(AssetManager::TryGet()->CreateAssetHandle(circleShader));
        s_Data->CircleMaterial = Material::Create(shaderHandle);
    }

    static void SetupTextBuffers()
    {
        s_Data->TextBuffer = s_Data->TextTmpBuffer = new TextVertex[s_Data->MaxTextVertices];
        s_Data->TextVertexBuffer =
          VertexBuffer::Create({ static_cast<uint32_t>(s_Data->MaxTextVertices * sizeof(TextVertex)), BufferUsage::BU_DYNAMIC_DRAW });

        const Ref<BufferLayout> layout = CreateRef<BufferLayout>(BufferLayout{ { ShaderDataType::Float3, "a_Position" },
                                                                               { ShaderDataType::Float4, "a_Color" },
                                                                               { ShaderDataType::Float2, "a_TexCoords" },
                                                                               { ShaderDataType::Float4, "a_OutlineColor" },
                                                                               { ShaderDataType::Float, "a_OutlineThickness" },
                                                                               { ShaderDataType::Float, "a_Weight" },
                                                                               { ShaderDataType::Float, "a_PixelRange" },
                                                                               { ShaderDataType::Float, "a_Softness" },
                                                                               { ShaderDataType::Float2, "a_LocalPosition" },
                                                                               { ShaderDataType::Float4, "a_ClipRect" },
                                                                               { ShaderDataType::Int, "a_Flags" },
                                                                               { ShaderDataType::Int, "a_ObjectId" } });
        s_Data->TextVertexBuffer->SetLayout(layout);

        const AssetHandle<Shader> shaderHandle = AssetManager::TryGet()->Load<Shader>("Resources/Shaders/Text.asset");
        // Ref<Shader> textShader = Importer::Get().Import<Shader>("Resources/Shaders/Text.glsl");
        // AssetManager::TryGet()->Save(textShader, "Resources/Shaders/Text.asset");
        // const AssetHandle<Shader> shaderHandle = static_asset_cast<Shader>(AssetManager::TryGet()->CreateAssetHandle(textShader));
        s_Data->TextMaterial = Material::Create(shaderHandle);
    }

    void Renderer2D::Init()
    {
        s_Data = new Renderer2DData();
        for (uint32_t i = 0; i < s_Data->TextureUniformNames.size(); i++)
        {
            s_Data->TextureUniformNames[i] = "u_Texture" + std::to_string(i + 1);
            s_Data->TextureUniformHashes[i] = HashedString(StringView(s_Data->TextureUniformNames[i]));
        }
        s_Data->Textures[0] = Texture::WHITE;
        SetupQuadBuffers();
        SetupCircleBuffers();
        SetupTextBuffers();
    }

    static void SetView(const glm::mat4& projection, const glm::mat4& view)
    {
        const glm::mat4 viewProjection = projection * view;
        s_Data->CircleMaterial->SetMatrix("u_ViewProjection"_hstr, viewProjection);
        s_Data->TextMaterial->SetMatrix("u_ViewProjection"_hstr, viewProjection);
        s_Data->QuadMaterial->SetMatrix("u_ViewProjection"_hstr, viewProjection);

        const glm::mat4 inverseView = glm::inverse(view);
        s_Data->CameraForward = glm::normalize(-glm::vec3(inverseView[2]));
        s_Data->CameraUp = glm::normalize(glm::vec3(inverseView[1]));
    }

    void Renderer2D::Begin(const Camera& camera, const glm::mat4& viewMatrix) { SetView(camera.GetProjection(), viewMatrix); }

    void Renderer2D::Begin(const glm::mat4& projection, const glm::mat4& view) { SetView(projection, view); }

    float Renderer2D::FindTexture(const Ref<Texture>& texture)
    {
        if (!texture)
            return 0;
        // TODO: Again why float?
        float textureSlot = 0.0f;

        for (uint8_t i = 1; i <= s_Data->TextureIndex; i++)
        {
            if (s_Data->Textures[i] == texture)
            {
                textureSlot = (float)(i + 1);
                break;
            }
        }

        if (textureSlot == 0)
        {
            if (s_Data->TextureIndex == 32) // TODO: not 32, use the system properties.
            {
                End();
                s_Data->QuadBuffer =
                  (VertexData*)s_Data->QuadVertexBuffer->Map(0, RENDERER_MAX_SPRITES * 4,
                                                             GpuLockOptions::WRITE_DISCARD); // TODO: Begin or something instead of this
            }
            s_Data->TextureIndex = (s_Data->TextureIndex + 1) % 32;
            s_Data->Textures[s_Data->TextureIndex] = texture;
            textureSlot = (float)s_Data->TextureIndex;
        }
        return textureSlot;
    }

    void Renderer2D::FillRect(const Rect2F& bounds, const glm::vec4& color, uint32_t entityId)
    {
        const glm::mat4 transform =
          glm::translate(glm::mat4(1.0f), { bounds.X, bounds.Y, 1.0f }) * glm::scale(glm::mat4(1.0f), { bounds.Width, bounds.Height, 1.0f });

        FillRect(transform, nullptr, color, entityId);
    }

    void Renderer2D::FillRect(const glm::mat4& transform, const Ref<Texture>& texture, const glm::vec4& color, uint32_t entityId)
    {
        const float ts = FindTexture(texture);

        for (uint32_t i = 0; i < 4; i++)
        {
            s_Data->QuadBuffer->Position = transform * QuadVertices[i];
            s_Data->QuadBuffer->Uv = QuadUv[i];
            s_Data->QuadBuffer->Tid = ts;
            s_Data->QuadBuffer->Color = color;
            s_Data->QuadBuffer->ObjectID = entityId;
            s_Data->QuadBuffer++;
        }

        s_Data->QuadVertexCount += 4;
        s_Data->QuadIndexCount += 6;
    }

    void Renderer2D::FillRect(const Rect2F& bounds, const Ref<Texture>& texture, const glm::vec4& color, uint32_t entityId)
    {
        const glm::mat4 transform =
          glm::translate(glm::mat4(1.0f), { bounds.X, bounds.Y, 1.0f }) * glm::scale(glm::mat4(1.0f), { bounds.Width, bounds.Height, 1.0f });

        FillRect(transform, texture, color, entityId);
    }

    void Renderer2D::DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness, float fade, int32_t entityId)
    {
        for (uint32_t i = 0; i < 4; i++)
        {
            s_Data->CircleBuffer->WorldPosition = transform * QuadVertices[i];
            s_Data->CircleBuffer->LocalPosition = QuadVertices[i] * 2.0f;
            s_Data->CircleBuffer->Color = color;
            s_Data->CircleBuffer->Thickness = thickness;
            s_Data->CircleBuffer->Fade = fade;
            s_Data->CircleBuffer->ObjectID = entityId;
            s_Data->CircleBuffer++;
        }
        s_Data->CircleIndexCount += 6;
        s_Data->CircleVertexCount += 4;
    }

    void Renderer2D::DrawLine(const glm::vec3& p1, const glm::vec3& p2, const glm::vec4& color, float thickness)
    {
        const glm::vec3 delta = p2 - p1;
        const float length = glm::length(delta);
        if (length <= std::numeric_limits<float>::epsilon() || thickness <= 0.0f)
            return;

        const glm::vec3 direction = delta / length;
        glm::vec3 side = glm::cross(direction, s_Data->CameraForward);
        if (glm::dot(side, side) <= std::numeric_limits<float>::epsilon())
            side = glm::cross(direction, s_Data->CameraUp);
        if (glm::dot(side, side) <= std::numeric_limits<float>::epsilon())
            side = glm::cross(direction, std::abs(direction.x) < 0.9f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f));
        side = glm::normalize(side);

        const glm::vec3 center = (p1 + p2) * 0.5f;
        glm::mat4 transform(1.0f);
        transform[0] = glm::vec4(delta, 0.0f);
        transform[1] = glm::vec4(side * thickness, 0.0f);
        transform[2] = glm::vec4(glm::cross(direction, side), 0.0f);
        transform[3] = glm::vec4(center, 1.0f);
        FillRect(transform, nullptr, color, std::numeric_limits<uint32_t>::max());
    }

    void Renderer2D::DrawRect(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color, float thickness)
    {
        const glm::vec3 p0 = glm::vec3(position.x - size.x * 0.5f, position.y - size.y * 0.5f, position.z);
        const glm::vec3 p1 = glm::vec3(position.x + size.x * 0.5f, position.y - size.y * 0.5f, position.z);
        const glm::vec3 p2 = glm::vec3(position.x + size.x * 0.5f, position.y + size.y * 0.5f, position.z);
        const glm::vec3 p3 = glm::vec3(position.x - size.x * 0.5f, position.y + size.y * 0.5f, position.z);

        DrawLine(p0, p1, color, thickness);
        DrawLine(p1, p2, color, thickness);
        DrawLine(p2, p3, color, thickness);
        DrawLine(p3, p0, color, thickness);
    }

    void Renderer2D::DrawRect(const glm::mat4& transform, const glm::vec4& color, float thickness)
    {
        // TODO: Add the thickness to the calculation since when it is a large value the corners aren't rendered
        // properly
        glm::vec3 lineVertices[4];
        for (uint32_t i = 0; i < 4; i++)
            lineVertices[i] = transform * QuadVertices[i];

        DrawLine(lineVertices[0], lineVertices[1], color, thickness);
        DrawLine(lineVertices[1], lineVertices[2], color, thickness);
        DrawLine(lineVertices[2], lineVertices[3], color, thickness);
        DrawLine(lineVertices[3], lineVertices[0], color, thickness);
    }

    void Renderer2D::DrawString(const TextComponent& textComponent, const glm::mat4& transform, int32_t entityId)
    {
        AssetHandle<Font> font = textComponent.Font;
        if (!font)
            font = FontManager::GetDefaultFont();
        if (!font || !font->IsValid() || textComponent.Text.empty())
            return;

        const TextLayoutResult layout = TextLayout::Build(textComponent, *font, s_Data->TextLayout);
        if (layout.LineCount == 0)
            return;

        Ref<Texture> activeAtlasTexture;
        auto selectFontAtlas = [&](const Font& sourceFont) {
            const Ref<Texture> atlasTexture = sourceFont.GetAtlasTexture();
            if (atlasTexture == nullptr)
                return false;

            if (s_Data->TextIndexCount > 0 && s_Data->FontAtlasTexture != atlasTexture)
            {
                FlushText();
                ResetTextBatch();
            }

            activeAtlasTexture = atlasTexture;
            s_Data->FontAtlasTexture = atlasTexture;
            return true;
        };
        if (!selectFontAtlas(*font))
            return;

        const msdfgen::FontMetrics& fontMetrics = *font->GetMetrics();
        const float atlasPixelRange = font->GetAtlasPixelRange();
        const float italicSlant = textComponent.FontStyle.IsSet(TextFontStyleBits::Italic) ? 0.2f : 0.0f;
        const float weight = textComponent.FontStyle.IsSet(TextFontStyleBits::Bold) ? 0.075f : 0.0f;
        const float outlineThickness = std::max(0.0f, textComponent.Thickness);
        const bool clipX = textComponent.ClipToBounds && textComponent.LayoutSize.x > 0.0f;
        const bool clipY = textComponent.ClipToBounds && textComponent.LayoutSize.y > 0.0f;
        const float extent = std::numeric_limits<float>::max() * 0.25f;
        const glm::vec4 clipRect(clipX ? 0.0f : -extent, clipY ? -textComponent.LayoutSize.y : -extent, clipX ? textComponent.LayoutSize.x : extent,
                                 clipY ? 0.0f : extent);
        const int32_t clipFlag = clipX || clipY ? 2 : 0;

        auto ensureTextCapacity = [&]() {
            if (s_Data->TextVertexCount + 4 <= s_Data->MaxTextVertices)
                return;
            FlushText();
            ResetTextBatch();
            s_Data->FontAtlasTexture = activeAtlasTexture;
        };

        auto emitQuad = [&](Array<glm::vec2, 4> positions, const Array<glm::vec2, 4>& uvs, const glm::vec4& color, const glm::vec4& outlineColor,
                            float outline, float glyphWeight, float pixelRange, float softness, int32_t flags, float baseline) {
            ensureTextCapacity();
            for (uint32_t vertexIndex = 0; vertexIndex < positions.size(); vertexIndex++)
            {
                positions[vertexIndex].x += italicSlant * (positions[vertexIndex].y - baseline) * ((flags & 1) == 0 ? 1.0f : 0.0f);
                TextVertex& vertex = *s_Data->TextBuffer++;
                vertex.Position = transform * glm::vec4(positions[vertexIndex], 0.0f, 1.0f);
                vertex.Color = color;
                vertex.TexCoords = uvs[vertexIndex];
                vertex.OutlineColor = outlineColor;
                vertex.OutlineThickness = outline;
                vertex.Weight = glyphWeight;
                vertex.PixelRange = pixelRange;
                vertex.Softness = std::max(0.0f, softness);
                vertex.LocalPosition = positions[vertexIndex];
                vertex.ClipRect = clipRect;
                vertex.Flags = flags | clipFlag;
                vertex.ObjectId = entityId;
            }
            s_Data->TextIndexCount += 6;
            s_Data->TextVertexCount += 4;
        };

        const glm::vec4 decorationColor = textComponent.UseCustomDecorationColor ? textComponent.DecorationColor : textComponent.Color;
        const float fontDecorationThickness = static_cast<float>(std::abs(fontMetrics.underlineThickness) * layout.GlyphScale);
        const float decorationThickness = textComponent.DecorationThickness > 0.0f
                                            ? textComponent.DecorationThickness
                                            : std::max(fontDecorationThickness, layout.FontSize / 36.0f * 0.035f);
        const Array<glm::vec2, 4> solidUvs = { glm::vec2(0.0f), glm::vec2(0.0f), glm::vec2(0.0f), glm::vec2(0.0f) };
        auto emitDecorations = [&]() {
            for (size_t lineIndex = 0; lineIndex < layout.LineCount; lineIndex++)
            {
                const TextLayoutLine& line = layout.Lines[lineIndex];
                if (line.Width <= 0.0f)
                    continue;

                auto emitDecoration = [&](float centerY) {
                    const glm::vec2 min(line.X, centerY - decorationThickness * 0.5f);
                    const glm::vec2 max(line.X + line.Width, centerY + decorationThickness * 0.5f);
                    const Array<glm::vec2, 4> positions = { min, { min.x, max.y }, max, { max.x, min.y } };
                    emitQuad(positions, solidUvs, decorationColor, decorationColor, 0.0f, 0.0f, atlasPixelRange, 0.0f, 1, line.Baseline);
                };

                if (textComponent.FontStyle.IsSet(TextFontStyleBits::Underline))
                    emitDecoration(line.Baseline + static_cast<float>(fontMetrics.underlineY * layout.GlyphScale) + textComponent.UnderlineOffset);
                if (textComponent.FontStyle.IsSet(TextFontStyleBits::Strikethrough))
                    emitDecoration(line.Baseline + static_cast<float>(fontMetrics.ascenderY * layout.GlyphScale * 0.32) +
                                   textComponent.StrikethroughOffset);
            }
        };

        auto emitGlyph = [&](const TextLayoutGlyph& layoutGlyph, const glm::vec4& color, const glm::vec4& glyphOutlineColor,
                             float glyphOutlineThickness, const glm::vec2& offset, float softness) {
            if (layoutGlyph.Glyph == nullptr)
                return;

            const Font* sourceFont = layoutGlyph.SourceFont != nullptr ? layoutGlyph.SourceFont : font.Get();
            if (sourceFont == nullptr || !selectFontAtlas(*sourceFont))
                return;

            const Ref<Texture>& atlasTexture = activeAtlasTexture;
            const float texelWidth = 1.0f / static_cast<float>(atlasTexture->GetWidth());
            const float texelHeight = 1.0f / static_cast<float>(atlasTexture->GetHeight());
            const float pixelRange = sourceFont->GetAtlasPixelRange();

            double atlasLeft = 0.0;
            double atlasBottom = 0.0;
            double atlasRight = 0.0;
            double atlasTop = 0.0;
            layoutGlyph.Glyph->getQuadAtlasBounds(atlasLeft, atlasBottom, atlasRight, atlasTop);
            const glm::vec2 uvMin(static_cast<float>(atlasLeft) * texelWidth, static_cast<float>(atlasBottom) * texelHeight);
            const glm::vec2 uvMax(static_cast<float>(atlasRight) * texelWidth, static_cast<float>(atlasTop) * texelHeight);

            double planeLeft = 0.0;
            double planeBottom = 0.0;
            double planeRight = 0.0;
            double planeTop = 0.0;
            layoutGlyph.Glyph->getQuadPlaneBounds(planeLeft, planeBottom, planeRight, planeTop);
            const glm::vec2 quadMin(layoutGlyph.PenPosition.x + static_cast<float>(planeLeft * layout.GlyphScale),
                                    layoutGlyph.PenPosition.y + static_cast<float>(planeBottom * layout.GlyphScale));
            const glm::vec2 quadMax(layoutGlyph.PenPosition.x + static_cast<float>(planeRight * layout.GlyphScale),
                                    layoutGlyph.PenPosition.y + static_cast<float>(planeTop * layout.GlyphScale));
            Array<glm::vec2, 4> positions = { quadMin, { quadMin.x, quadMax.y }, quadMax, { quadMax.x, quadMin.y } };
            for (glm::vec2& position : positions)
                position += offset;
            const Array<glm::vec2, 4> uvs = { uvMin, { uvMin.x, uvMax.y }, uvMax, { uvMax.x, uvMin.y } };
            emitQuad(positions, uvs, color, glyphOutlineColor, glyphOutlineThickness, weight, pixelRange, softness, 0,
                     layoutGlyph.PenPosition.y + offset.y);
        };

        if (textComponent.ShadowColor.a > 0.0f)
        {
            for (size_t glyphIndex = 0; glyphIndex < layout.GlyphCount; glyphIndex++)
                emitGlyph(layout.Glyphs[glyphIndex], textComponent.ShadowColor, textComponent.ShadowColor, 0.0f, textComponent.ShadowOffset,
                          textComponent.ShadowSoftness);
        }

        emitDecorations();

        for (size_t glyphIndex = 0; glyphIndex < layout.GlyphCount; glyphIndex++)
            emitGlyph(layout.Glyphs[glyphIndex], textComponent.Color, textComponent.OutlineColor, outlineThickness, glm::vec2(0.0f), 0.0f);
    }

    void Renderer2D::End()
    {
        Flush();
        s_Data->QuadBuffer = s_Data->QuadTmpBuffer;
        s_Data->QuadIndexCount = 0;
        s_Data->QuadVertexCount = 0;
        s_Data->TextureIndex = 0;

        s_Data->CircleBuffer = s_Data->CircleTmpBuffer;
        s_Data->CircleIndexCount = 0;
        s_Data->CircleVertexCount = 0;

        ResetTextBatch();
    }

    static void FlushQuads()
    {
        ZoneScopedN("FlushQuads");
        if (s_Data->QuadIndexCount > 0)
        {
            RenderAPI::TryGet()->SetGraphicsPipeline(s_Data->QuadMaterial->GetGraphicsPipeline());
            RenderAPI::TryGet()->SetVertexLayout(s_Data->QuadVertexBuffer->GetLayout());
            for (uint32_t i = 0; i < 8; i++)
            {
                if (s_Data->Textures[i])
                    s_Data->QuadMaterial->SetTexture(s_Data->TextureUniformHashes[i], s_Data->Textures[i]);
                else
                    s_Data->QuadMaterial->SetTexture(s_Data->TextureUniformHashes[i], s_Data->Textures[0]);
            }
            RenderAPI::TryGet()->SetUniforms(s_Data->QuadMaterial->GetUniformParams());

            RenderAPI::TryGet()->SetVertexBuffers(0, &s_Data->QuadVertexBuffer, 1);
            RenderAPI::TryGet()->SetIndexBuffer(s_Data->QuadIndexBuffer);
            s_Data->QuadVertexBuffer->WriteData(0, s_Data->QuadVertexCount * sizeof(VertexData), s_Data->QuadTmpBuffer, BWT_DISCARD);
            RenderAPI::TryGet()->DrawIndexed(0, s_Data->QuadIndexCount, 0, s_Data->QuadVertexCount);
        }
    }

    static void FlushCircles()
    {
        if (s_Data->CircleIndexCount > 0)
        {
            // This will flush buffers
            RenderAPI::TryGet()->SetGraphicsPipeline(s_Data->CircleMaterial->GetGraphicsPipeline());
            RenderAPI::TryGet()->SetVertexLayout(s_Data->CircleVertexBuffer->GetLayout());
            RenderAPI::TryGet()->SetUniforms(s_Data->CircleMaterial->GetUniformParams());

            RenderAPI::TryGet()->SetVertexBuffers(0, &s_Data->CircleVertexBuffer, 1);
            s_Data->CircleVertexBuffer->WriteData(0, s_Data->CircleVertexCount * sizeof(CircleVertex), s_Data->CircleTmpBuffer, BWT_DISCARD);
            RenderAPI::TryGet()->DrawIndexed(0, s_Data->CircleIndexCount, 0, s_Data->CircleVertexCount);
        }
    }

    static void FlushText()
    {
        ZoneScopedN("FlushText");
        if (s_Data->TextIndexCount > 0)
        {
            s_Data->TextMaterial->SetTexture("u_FontAtlas"_hstr, s_Data->FontAtlasTexture);

            RenderAPI::TryGet()->SetGraphicsPipeline(s_Data->TextMaterial->GetGraphicsPipeline());
            RenderAPI::TryGet()->SetVertexLayout(s_Data->TextVertexBuffer->GetLayout());
            RenderAPI::TryGet()->SetUniforms(s_Data->TextMaterial->GetUniformParams());

            RenderAPI::TryGet()->SetVertexBuffers(0, &s_Data->TextVertexBuffer, 1);
            RenderAPI::TryGet()->SetIndexBuffer(s_Data->QuadIndexBuffer);
            s_Data->TextVertexBuffer->WriteData(0, s_Data->TextVertexCount * sizeof(TextVertex), s_Data->TextTmpBuffer, BWT_DISCARD);

            RenderAPI::TryGet()->DrawIndexed(0, s_Data->TextIndexCount, 0, s_Data->TextVertexCount);
        }
    }

    void Renderer2D::Flush()
    {
        ZoneScopedN("Renderer2D::Flush");
        FlushQuads();
        FlushCircles();
        FlushText();
    }

    void Renderer2D::Shutdown()
    {
        if (!s_Data)
            return;
        delete[] s_Data->QuadTmpBuffer;
        delete[] s_Data->CircleTmpBuffer;
        delete[] s_Data->TextTmpBuffer;

        delete s_Data;
        s_Data = nullptr;
    }
} // namespace Crowny

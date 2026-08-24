#include "cwpch.h"

#include "Crowny/Renderer/Renderer2D.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/UTF8.h"
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

    const std::array<glm::vec2, 4> QuadUv = { { glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f),
                                                glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f) } };
    const std::array<glm::vec4, 4> QuadVertices = { {
        glm::vec4(-0.5f, -0.5f, 0.0f, 1.0f), glm::vec4(0.5f, -0.5f, 0.0f, 1.0f),
        glm::vec4(0.5f, 0.5f, 0.0f, 1.0f), glm::vec4(-0.5f, 0.5f, 0.0f, 1.0f)
    } };

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
        s_Data->CircleVertexBuffer = VertexBuffer::Create(
            { static_cast<uint32_t>(s_Data->MaxLineVertices * sizeof(CircleVertex)), BufferUsage::BU_DYNAMIC_DRAW });
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
        s_Data->TextVertexBuffer = VertexBuffer::Create(
            { static_cast<uint32_t>(s_Data->MaxTextVertices * sizeof(TextVertex)), BufferUsage::BU_DYNAMIC_DRAW });

        const Ref<BufferLayout> layout = CreateRef<BufferLayout>(BufferLayout{ { ShaderDataType::Float3, "a_Position" },
                                                                               { ShaderDataType::Float4, "a_Color" },
                                                                               { ShaderDataType::Float2, "a_TexCoords" },
                                                                               { ShaderDataType::Float4, "a_OutlineColor" },
                                                                               { ShaderDataType::Float, "a_OutlineThickness" },
                                                                               { ShaderDataType::Float, "a_Weight" },
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

    void Renderer2D::Begin(const glm::mat4& projection, const glm::mat4& view)
    {
        SetView(projection, view);
    }

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

    namespace
    {
        constexpr size_t INVALID_TEXT_INDEX = std::numeric_limits<size_t>::max();

        bool IsTextWhitespace(char32_t codePoint)
        {
            return codePoint == U'\t' || codePoint == U' ' || codePoint == 0x00A0 || codePoint == 0x1680 ||
                   (codePoint >= 0x2000 && codePoint <= 0x200A) || codePoint == 0x202F || codePoint == 0x205F || codePoint == 0x3000;
        }

        bool IsBreakableWhitespace(char32_t codePoint) { return IsTextWhitespace(codePoint) && codePoint != 0x00A0 && codePoint != 0x202F; }

        bool IsCombiningMark(char32_t codePoint)
        {
            return (codePoint >= 0x0300 && codePoint <= 0x036F) || (codePoint >= 0x1AB0 && codePoint <= 0x1AFF) ||
                   (codePoint >= 0x1DC0 && codePoint <= 0x1DFF) || (codePoint >= 0x20D0 && codePoint <= 0x20FF) ||
                   (codePoint >= 0xFE20 && codePoint <= 0xFE2F);
        }

        bool IsCJKCharacter(char32_t codePoint)
        {
            return (codePoint >= 0x2E80 && codePoint <= 0xA4CF) || (codePoint >= 0xAC00 && codePoint <= 0xD7AF) ||
                   (codePoint >= 0xF900 && codePoint <= 0xFAFF) || (codePoint >= 0x20000 && codePoint <= 0x323AF);
        }

        bool IsClosingPunctuation(char32_t codePoint)
        {
            switch (codePoint)
            {
            case U')':
            case U']':
            case U'}':
            case U',':
            case U'.':
            case U'!':
            case U'?':
            case U':':
            case U';':
            case 0x3001:
            case 0x3002:
            case 0xFF01:
            case 0xFF09:
            case 0xFF0C:
            case 0xFF0E:
            case 0xFF1A:
            case 0xFF1B:
            case 0xFF1F:
                return true;
            default:
                return false;
            }
        }

        bool IsExplicitBreakCharacter(char32_t codePoint)
        {
            return codePoint == U'-' || codePoint == U'/' || codePoint == 0x00AD || codePoint == 0x200B;
        }

        double TokenAdvance(const TextLayoutToken& token, double penX, double glyphScale, const TextComponent& component,
                            const TextLayoutFontData& fontData)
        {
            if (token.NewLine || token.CodePoint == 0x200B || token.CodePoint == 0x00AD)
                return 0.0;

            if (token.CodePoint == U'\t')
            {
                const double space = std::max(0.000001, fontData.SpaceAdvance * glyphScale + component.CharacterSpacing + component.WordSpacing);
                const double tabStop = space * static_cast<double>(fontData.TabWidth);
                const double remainder = std::fmod(std::max(0.0, penX), tabStop);
                return remainder <= 0.000001 ? tabStop : tabStop - remainder;
            }

            double advance = token.Advance * glyphScale + component.CharacterSpacing;
            if (token.WhiteSpace)
                advance += component.WordSpacing;
            return std::max(0.0, advance);
        }

        size_t TrimTrailingWhitespace(const FrameVector<TextLayoutToken>& tokens, size_t begin, size_t end)
        {
            while (end > begin && (tokens[end - 1].WhiteSpace || tokens[end - 1].Invisible))
                end--;
            return end;
        }

        double MeasureTokenRange(const FrameVector<TextLayoutToken>& tokens, size_t begin, size_t end, double glyphScale,
                                 const TextComponent& component, const TextLayoutFontData& fontData)
        {
            end = TrimTrailingWhitespace(tokens, begin, end);
            double pen = 0.0;
            double width = 0.0;
            for (size_t i = begin; i < end; i++)
            {
                const TextLayoutToken& token = tokens[i];
                const double advance = TokenAdvance(token, pen, glyphScale, component, fontData);
                pen += advance;
                if (!token.WhiteSpace && !token.Invisible)
                    width = std::max(0.0, pen - static_cast<double>(component.CharacterSpacing));
            }
            return width;
        }

        void DecodeText(const TextComponent& component, const Font& font, TextLayoutScratch& scratch)
        {
            scratch.Tokens.Reset();
            size_t offset = 0;
            char32_t codePoint = 0;
            bool previousWasCarriageReturn = false;
            while (UTF8::NextCodePoint(component.Text, offset, codePoint))
            {
                if (codePoint == U'\n' && previousWasCarriageReturn)
                {
                    previousWasCarriageReturn = false;
                    continue;
                }

                previousWasCarriageReturn = codePoint == U'\r';
                if (previousWasCarriageReturn)
                    codePoint = U'\n';

                TextLayoutToken& token = scratch.Tokens.Acquire();
                token = {};
                token.CodePoint = codePoint;
                token.NewLine = codePoint == U'\n';
                token.WhiteSpace = IsTextWhitespace(codePoint);
                token.Invisible = codePoint == 0x200B || codePoint == 0x00AD;
                token.CombiningMark = IsCombiningMark(codePoint);
                if (!token.NewLine && !token.WhiteSpace && !token.Invisible)
                {
                    token.Glyph = font.GetGlyph(codePoint);
                    token.Renderable = token.Glyph != nullptr;
                }
            }

            for (size_t i = 0; i < scratch.Tokens.Size(); i++)
            {
                TextLayoutToken& token = scratch.Tokens[i];
                char32_t nextCodePoint = 0;
                if (i + 1 < scratch.Tokens.Size() && !scratch.Tokens[i + 1].NewLine)
                    nextCodePoint = scratch.Tokens[i + 1].CodePoint;

                if (!token.NewLine && !token.Invisible && token.CodePoint != U'\t')
                    token.Advance = font.GetAdvance(token.CodePoint, nextCodePoint, component.UseKerning);

                token.BreakAfter = IsBreakableWhitespace(token.CodePoint) || IsExplicitBreakCharacter(token.CodePoint);
                if (IsCJKCharacter(token.CodePoint) && i + 1 < scratch.Tokens.Size())
                {
                    const TextLayoutToken& next = scratch.Tokens[i + 1];
                    token.BreakAfter |= !next.NewLine && !next.CombiningMark && !IsClosingPunctuation(next.CodePoint);
                }
            }
        }

        void AddLayoutLine(TextLayoutScratch& scratch, size_t begin, size_t end, bool paragraphEnd)
        {
            TextLayoutLine& line = scratch.Lines.Acquire();
            line = {};
            line.TokenStart = begin;
            line.TokenEnd = end;
            line.RenderTokenEnd = end;
            line.ParagraphEnd = paragraphEnd;
        }

        void BuildLineRanges(const TextComponent& component, const TextLayoutFontData& fontData, TextLayoutScratch& scratch, double glyphScale)
        {
            scratch.Lines.Reset();
            const bool wrap = component.Wrapping && component.LayoutSize.x > 0.0f;
            const double widthLimit = std::max(0.0f, component.LayoutSize.x);
            const size_t tokenCount = scratch.Tokens.Size();
            size_t lineStart = 0;

            while (lineStart < tokenCount)
            {
                if (scratch.Tokens[lineStart].NewLine)
                {
                    AddLayoutLine(scratch, lineStart, lineStart, true);
                    lineStart++;
                    if (lineStart == tokenCount)
                        AddLayoutLine(scratch, lineStart, lineStart, true);
                    continue;
                }

                double pen = 0.0;
                size_t lastBreak = INVALID_TEXT_INDEX;
                size_t i = lineStart;
                bool emitted = false;
                while (i < tokenCount && !scratch.Tokens[i].NewLine)
                {
                    const TextLayoutToken& token = scratch.Tokens[i];
                    const double advance = TokenAdvance(token, pen, glyphScale, component, fontData);
                    const bool exceedsWidth = wrap && pen + advance > widthLimit + 0.000001;
                    const bool canBreakBeforeCurrent = component.WrapMode == TextWrapMode::Character ||
                                                       component.WrapMode == TextWrapMode::WordThenCharacter || lastBreak != INVALID_TEXT_INDEX;
                    if (exceedsWidth && canBreakBeforeCurrent && i > lineStart)
                    {
                        size_t lineEnd = i;
                        size_t nextLine = i;
                        if (component.WrapMode != TextWrapMode::Character && lastBreak != INVALID_TEXT_INDEX && lastBreak > lineStart)
                        {
                            lineEnd = lastBreak;
                            nextLine = lastBreak;
                        }
                        while (nextLine < tokenCount && !scratch.Tokens[nextLine].NewLine &&
                               IsBreakableWhitespace(scratch.Tokens[nextLine].CodePoint))
                            nextLine++;
                        AddLayoutLine(scratch, lineStart, lineEnd, false);
                        lineStart = nextLine;
                        emitted = true;
                        break;
                    }

                    pen += advance;
                    i++;
                    if (token.BreakAfter)
                        lastBreak = i;

                    if (exceedsWidth && component.WrapMode == TextWrapMode::Word && token.BreakAfter)
                    {
                        AddLayoutLine(scratch, lineStart, i, false);
                        lineStart = i;
                        while (lineStart < tokenCount && !scratch.Tokens[lineStart].NewLine &&
                               IsBreakableWhitespace(scratch.Tokens[lineStart].CodePoint))
                            lineStart++;
                        emitted = true;
                        break;
                    }

                    if (exceedsWidth && component.WrapMode != TextWrapMode::Word)
                    {
                        AddLayoutLine(scratch, lineStart, i, false);
                        lineStart = i;
                        emitted = true;
                        break;
                    }
                }

                if (emitted)
                    continue;

                const bool paragraphEnd = i < tokenCount && scratch.Tokens[i].NewLine;
                AddLayoutLine(scratch, lineStart, i, true);
                lineStart = paragraphEnd ? i + 1 : i;
                if (paragraphEnd && lineStart == tokenCount)
                    AddLayoutLine(scratch, lineStart, lineStart, true);
            }
        }

        struct NaturalTextMetrics
        {
            double Width = 0.0;
            double Height = 0.0;
            double GlyphScale = 0.0;
            double LineAdvance = 0.0;
            bool OverflowX = false;
            bool OverflowY = false;
        };

        NaturalTextMetrics MeasureNaturalLayout(const TextComponent& component, const TextLayoutFontData& fontData, TextLayoutScratch& scratch,
                                                double fontSize)
        {
            NaturalTextMetrics result;
            const double metricHeight = fontData.Ascender - fontData.Descender;
            result.GlyphScale = (fontSize / 36.0) / metricHeight;
            result.LineAdvance = std::max(metricHeight * result.GlyphScale * 0.1, fontData.LineHeight * result.GlyphScale + component.LineSpacing);
            BuildLineRanges(component, fontData, scratch, result.GlyphScale);

            double baseline = 0.0;
            for (size_t i = 0; i < scratch.Lines.Size(); i++)
            {
                TextLayoutLine& line = scratch.Lines[i];
                line.Baseline = static_cast<float>(baseline);
                line.NaturalWidth =
                  static_cast<float>(MeasureTokenRange(scratch.Tokens, line.TokenStart, line.TokenEnd, result.GlyphScale, component, fontData));
                result.Width = std::max(result.Width, static_cast<double>(line.NaturalWidth));
                if (i + 1 < scratch.Lines.Size())
                {
                    baseline -= result.LineAdvance;
                    if (line.ParagraphEnd)
                        baseline -= component.ParagraphSpacing;
                }
            }

            if (!scratch.Lines.Empty())
            {
                const double bottom = scratch.Lines[scratch.Lines.Size() - 1].Baseline + fontData.Descender * result.GlyphScale;
                result.Height = fontData.Ascender * result.GlyphScale - bottom;
            }
            result.OverflowX = component.LayoutSize.x > 0.0f && result.Width > component.LayoutSize.x + 0.000001;
            result.OverflowY = component.LayoutSize.y > 0.0f && result.Height > component.LayoutSize.y + 0.000001;
            return result;
        }

        size_t FitTokenRange(const TextComponent& component, const TextLayoutFontData& fontData, const TextLayoutScratch& scratch,
                             const TextLayoutLine& line, double glyphScale, double widthLimit, bool reserveEllipsis)
        {
            const double ellipsisWidth = reserveEllipsis ? fontData.EllipsisAdvance * glyphScale : 0.0;
            const double available = std::max(0.0, widthLimit - ellipsisWidth);
            double pen = 0.0;
            size_t result = line.TokenStart;
            for (size_t i = line.TokenStart; i < line.TokenEnd; i++)
            {
                const TextLayoutToken& token = scratch.Tokens[i];
                const double advance = TokenAdvance(token, pen, glyphScale, component, fontData);
                const double candidatePen = pen + advance;
                const double candidateWidth =
                  !token.WhiteSpace && !token.Invisible ? std::max(0.0, candidatePen - static_cast<double>(component.CharacterSpacing)) : pen;
                if (candidateWidth > available + 0.000001)
                    break;
                pen = candidatePen;
                result = i + 1;
            }
            return TrimTrailingWhitespace(scratch.Tokens, line.TokenStart, result);
        }

        uint32_t CountWhitespaceGaps(const TextLayoutScratch& scratch, size_t begin, size_t end)
        {
            uint32_t count = 0;
            for (size_t i = begin; i < end; i++)
            {
                if (!scratch.Tokens[i].WhiteSpace)
                    continue;
                const bool hasTextBefore = i > begin;
                const bool atEndOfRun = i + 1 == end || !scratch.Tokens[i + 1].WhiteSpace;
                if (hasTextBefore && atEndOfRun && i + 1 < end)
                    count++;
            }
            return count;
        }

        uint32_t CountInterCharacterGaps(const TextLayoutScratch& scratch, size_t begin, size_t end)
        {
            uint32_t glyphCount = 0;
            for (size_t i = begin; i < end; i++)
            {
                const TextLayoutToken& token = scratch.Tokens[i];
                if (token.Renderable && !token.WhiteSpace && !token.Invisible && !token.CombiningMark)
                    glyphCount++;
            }
            return glyphCount > 1 ? glyphCount - 1 : 0;
        }
    } // namespace

    TextLayoutResult TextLayout::Build(const TextComponent& component, const Font& font, TextLayoutScratch& scratch)
    {
        TextLayoutResult result;
        const msdfgen::FontMetrics* fontMetrics = font.GetMetrics();
        if (!font.IsValid() || fontMetrics == nullptr || component.Text.empty())
            return result;

        scratch.Reset();
        scratch.Reserve(component.Text.size());
        DecodeText(component, font, scratch);
        const TextLayoutFontData fontData{ fontMetrics->ascenderY,
                                           fontMetrics->descenderY,
                                           fontMetrics->lineHeight,
                                           font.GetAdvance(U' ', 0, false),
                                           font.GetAdvance(0x2026, 0, component.UseKerning),
                                           font.GetTabWidth(),
                                           font.GetGlyph(0x2026) };
        return BuildPrepared(component, fontData, scratch);
    }

    TextLayoutResult TextLayout::BuildPrepared(const TextComponent& component, const TextLayoutFontData& fontData, TextLayoutScratch& scratch)
    {
        TextLayoutResult result;
        const double metricHeight = fontData.Ascender - fontData.Descender;
        if (scratch.Tokens.Empty() || metricHeight <= std::numeric_limits<double>::epsilon())
            return result;

        double fontSize = std::max(0.0f, component.Size);
        if (component.AutoSize)
        {
            const double minimumSize = std::max(0.01f, std::min(component.AutoSizeMin, component.AutoSizeMax));
            const double maximumSize = std::max(minimumSize, static_cast<double>(std::max(component.AutoSizeMin, component.AutoSizeMax)));
            if (component.LayoutSize.x > 0.0f || component.LayoutSize.y > 0.0f)
            {
                double low = minimumSize;
                double high = maximumSize;
                for (uint32_t iteration = 0; iteration < 12; iteration++)
                {
                    const double candidate = (low + high) * 0.5;
                    const NaturalTextMetrics metrics = MeasureNaturalLayout(component, fontData, scratch, candidate);
                    const bool lineLimitFits = component.MaxLines == 0 || scratch.Lines.Size() <= component.MaxLines;
                    if (!metrics.OverflowX && !metrics.OverflowY && lineLimitFits)
                        low = candidate;
                    else
                        high = candidate;
                }
                fontSize = low;
            }
            else
                fontSize = std::clamp(static_cast<double>(component.Size), minimumSize, maximumSize);
        }
        if (fontSize <= 0.0)
            return result;

        const NaturalTextMetrics natural = MeasureNaturalLayout(component, fontData, scratch, fontSize);
        result.FontSize = static_cast<float>(fontSize);
        result.GlyphScale = static_cast<float>(natural.GlyphScale);
        result.LineAdvance = static_cast<float>(natural.LineAdvance);
        result.OverflowedHorizontally = natural.OverflowX;
        result.OverflowedVertically = natural.OverflowY;

        size_t visibleLineCount = scratch.Lines.Size();
        if (component.MaxLines > 0)
            visibleLineCount = std::min(visibleLineCount, static_cast<size_t>(component.MaxLines));

        if (component.Overflow != TextOverflow::Overflow && component.LayoutSize.y > 0.0f)
        {
            size_t fittingLines = 0;
            for (size_t i = 0; i < visibleLineCount; i++)
            {
                const double bottom = scratch.Lines[i].Baseline + fontData.Descender * natural.GlyphScale;
                const double height = fontData.Ascender * natural.GlyphScale - bottom;
                if (height > component.LayoutSize.y + 0.000001 && i > 0)
                    break;
                fittingLines = i + 1;
            }
            visibleLineCount = fittingLines;
        }

        const bool hiddenLines = visibleLineCount < scratch.Lines.Size();
        result.Truncated = hiddenLines;
        if (visibleLineCount == 0)
            return result;

        const double ellipsisWidth = fontData.EllipsisAdvance * natural.GlyphScale;
        double maximumLineWidth = 0.0;
        for (size_t lineIndex = 0; lineIndex < visibleLineCount; lineIndex++)
        {
            TextLayoutLine& line = scratch.Lines[lineIndex];
            line.RenderTokenEnd = TrimTrailingWhitespace(scratch.Tokens, line.TokenStart, line.TokenEnd);
            const bool horizontalOverflow = component.LayoutSize.x > 0.0f && line.NaturalWidth > component.LayoutSize.x + 0.000001;
            const bool verticalEllipsis = hiddenLines && lineIndex + 1 == visibleLineCount;
            line.Ellipsized = component.Overflow == TextOverflow::Ellipses && (horizontalOverflow || verticalEllipsis);

            if (component.LayoutSize.x > 0.0f && component.Overflow != TextOverflow::Overflow && (horizontalOverflow || line.Ellipsized))
            {
                line.RenderTokenEnd = FitTokenRange(component, fontData, scratch, line, natural.GlyphScale, component.LayoutSize.x, line.Ellipsized);
                result.Truncated |= line.RenderTokenEnd < TrimTrailingWhitespace(scratch.Tokens, line.TokenStart, line.TokenEnd);
            }

            line.Width =
              static_cast<float>(MeasureTokenRange(scratch.Tokens, line.TokenStart, line.RenderTokenEnd, natural.GlyphScale, component, fontData) +
                                 (line.Ellipsized ? ellipsisWidth : 0.0));
            const bool justify = component.LayoutSize.x > 0.0f && !line.Ellipsized &&
                                 (component.HorizontalAlignment == TextHorizontalAlignment::Flush ||
                                  (component.HorizontalAlignment == TextHorizontalAlignment::Justified && !line.ParagraphEnd));
            if (justify && line.Width < component.LayoutSize.x)
            {
                line.ExpandableGaps = CountWhitespaceGaps(scratch, line.TokenStart, line.RenderTokenEnd);
                if (line.ExpandableGaps == 0)
                    line.ExpandableGaps = CountInterCharacterGaps(scratch, line.TokenStart, line.RenderTokenEnd);
                if (line.ExpandableGaps > 0)
                    line.Width = component.LayoutSize.x;
            }

            const bool boundedWidth = component.LayoutSize.x > 0.0f;
            const double horizontalSpace = boundedWidth ? component.LayoutSize.x : 0.0;
            switch (component.HorizontalAlignment)
            {
            case TextHorizontalAlignment::Center:
                line.X = static_cast<float>((horizontalSpace - line.Width) * 0.5);
                break;
            case TextHorizontalAlignment::Right:
                line.X = static_cast<float>(horizontalSpace - line.Width);
                break;
            default:
                line.X = 0.0f;
                break;
            }
            maximumLineWidth = std::max(maximumLineWidth, static_cast<double>(line.Width));
        }

        const double blockTop = fontData.Ascender * natural.GlyphScale;
        const double blockBottom = scratch.Lines[visibleLineCount - 1].Baseline + fontData.Descender * natural.GlyphScale;
        const double blockHeight = blockTop - blockBottom;
        const bool boundedHeight = component.LayoutSize.y > 0.0f;
        double verticalOffset = 0.0;
        switch (component.VerticalAlignment)
        {
        case TextVerticalAlignment::Top:
            verticalOffset = -blockTop;
            break;
        case TextVerticalAlignment::Middle:
            verticalOffset = (boundedHeight ? -component.LayoutSize.y * 0.5 : 0.0) - (blockTop + blockBottom) * 0.5;
            break;
        case TextVerticalAlignment::Bottom:
            verticalOffset = (boundedHeight ? -component.LayoutSize.y : 0.0) - blockBottom;
            break;
        case TextVerticalAlignment::Midline:
            verticalOffset =
              (boundedHeight ? -component.LayoutSize.y * 0.5 : 0.0) - (fontData.Ascender + fontData.Descender) * natural.GlyphScale * 0.5;
            break;
        case TextVerticalAlignment::Baseline:
        default:
            break;
        }
        for (size_t i = 0; i < visibleLineCount; i++)
            scratch.Lines[i].Baseline += static_cast<float>(verticalOffset);

        scratch.Glyphs.Reset();
        for (size_t lineIndex = 0; lineIndex < visibleLineCount; lineIndex++)
        {
            TextLayoutLine& line = scratch.Lines[lineIndex];
            line.FirstGlyph = scratch.Glyphs.Size();
            double pen = line.X;
            const double relativeWidth = line.Width;
            const double naturalOutputWidth =
              MeasureTokenRange(scratch.Tokens, line.TokenStart, line.RenderTokenEnd, natural.GlyphScale, component, fontData) +
              (line.Ellipsized ? ellipsisWidth : 0.0);
            const double gapExpansion = line.ExpandableGaps > 0 ? (relativeWidth - naturalOutputWidth) / line.ExpandableGaps : 0.0;
            const bool whitespaceGaps = CountWhitespaceGaps(scratch, line.TokenStart, line.RenderTokenEnd) > 0;
            uint32_t expandedCharacterGaps = 0;

            for (size_t i = line.TokenStart; i < line.RenderTokenEnd; i++)
            {
                const TextLayoutToken& token = scratch.Tokens[i];
                const double advance = TokenAdvance(token, pen - line.X, natural.GlyphScale, component, fontData);
                if (token.Renderable && !token.WhiteSpace && !token.Invisible)
                {
                    TextLayoutGlyph& glyph = scratch.Glyphs.Acquire();
                    glyph.CodePoint = token.CodePoint;
                    glyph.Glyph = token.Glyph;
                    glyph.PenPosition = { static_cast<float>(pen), line.Baseline };
                    glyph.Advance = static_cast<float>(advance);
                    glyph.LineIndex = static_cast<uint32_t>(lineIndex);
                }
                pen += advance;

                if (gapExpansion > 0.0)
                {
                    if (whitespaceGaps)
                    {
                        const bool endOfWhitespaceRun = token.WhiteSpace && i + 1 < line.RenderTokenEnd && !scratch.Tokens[i + 1].WhiteSpace;
                        if (endOfWhitespaceRun)
                            pen += gapExpansion;
                    }
                    else if (token.Renderable && !token.CombiningMark && expandedCharacterGaps < line.ExpandableGaps)
                    {
                        pen += gapExpansion;
                        expandedCharacterGaps++;
                    }
                }
            }

            if (line.Ellipsized)
            {
                TextLayoutGlyph& glyph = scratch.Glyphs.Acquire();
                glyph.CodePoint = 0x2026;
                glyph.Glyph = fontData.EllipsisGlyph;
                glyph.PenPosition = { static_cast<float>(pen), line.Baseline };
                glyph.Advance = static_cast<float>(ellipsisWidth);
                glyph.LineIndex = static_cast<uint32_t>(lineIndex);
            }
            line.GlyphCount = scratch.Glyphs.Size() - line.FirstGlyph;
        }

        result.Glyphs = scratch.Glyphs.Empty() ? nullptr : scratch.Glyphs.begin();
        result.Lines = scratch.Lines.begin();
        result.GlyphCount = scratch.Glyphs.Size();
        result.LineCount = visibleLineCount;
        result.Size = { static_cast<float>(maximumLineWidth), static_cast<float>(blockHeight) };
        return result;
    }

    void Renderer2D::DrawString(const TextComponent& textComponent, const glm::mat4& transform, int32_t entityId)
    {
        AssetHandle<Font> font = textComponent.Font;
        if (!font)
            font = Font::GetDefaultFont();
        if (!font || !font->IsValid() || textComponent.Text.empty())
            return;

        const TextLayoutResult layout = TextLayout::Build(textComponent, *font, s_Data->TextLayout);
        if (layout.LineCount == 0)
            return;

        const Ref<Texture> fontAtlasTexture = font->GetAtlasTexture();
        if (s_Data->TextIndexCount > 0 && s_Data->FontAtlasTexture != fontAtlasTexture)
        {
            FlushText();
            ResetTextBatch();
        }
        s_Data->FontAtlasTexture = fontAtlasTexture;

        const msdfgen::FontMetrics& fontMetrics = *font->GetMetrics();
        const float texelWidth = 1.0f / static_cast<float>(fontAtlasTexture->GetWidth());
        const float texelHeight = 1.0f / static_cast<float>(fontAtlasTexture->GetHeight());
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
            s_Data->FontAtlasTexture = fontAtlasTexture;
        };

        auto emitQuad = [&](Array<glm::vec2, 4> positions, const Array<glm::vec2, 4>& uvs, const glm::vec4& color, const glm::vec4& outlineColor,
                            float outline, float glyphWeight, int32_t flags, float baseline) {
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
        for (size_t lineIndex = 0; lineIndex < layout.LineCount; lineIndex++)
        {
            const TextLayoutLine& line = layout.Lines[lineIndex];
            if (line.Width <= 0.0f)
                continue;

            auto emitDecoration = [&](float centerY) {
                const glm::vec2 min(line.X, centerY - decorationThickness * 0.5f);
                const glm::vec2 max(line.X + line.Width, centerY + decorationThickness * 0.5f);
                const Array<glm::vec2, 4> positions = { min, { min.x, max.y }, max, { max.x, min.y } };
                emitQuad(positions, solidUvs, decorationColor, decorationColor, 0.0f, 0.0f, 1, line.Baseline);
            };

            if (textComponent.FontStyle.IsSet(TextFontStyleBits::Underline))
                emitDecoration(line.Baseline + static_cast<float>(fontMetrics.underlineY * layout.GlyphScale) + textComponent.UnderlineOffset);
            if (textComponent.FontStyle.IsSet(TextFontStyleBits::Strikethrough))
                emitDecoration(line.Baseline + static_cast<float>(fontMetrics.ascenderY * layout.GlyphScale * 0.32) +
                               textComponent.StrikethroughOffset);
        }

        for (size_t glyphIndex = 0; glyphIndex < layout.GlyphCount; glyphIndex++)
        {
            const TextLayoutGlyph& layoutGlyph = layout.Glyphs[glyphIndex];
            if (layoutGlyph.Glyph == nullptr)
                continue;

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
            const Array<glm::vec2, 4> positions = { quadMin, { quadMin.x, quadMax.y }, quadMax, { quadMax.x, quadMin.y } };
            const Array<glm::vec2, 4> uvs = { uvMin, { uvMin.x, uvMax.y }, uvMax, { uvMax.x, uvMin.y } };
            emitQuad(positions, uvs, textComponent.Color, textComponent.OutlineColor, outlineThickness, weight, 0, layoutGlyph.PenPosition.y);
        }
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

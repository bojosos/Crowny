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
#include "Crowny/Renderer/Material.h"
#include "Crowny/Utils/ShaderCompiler.h"

#include "MSDFData.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Crowny
{

    constexpr glm::vec2 QuadUv[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
    constexpr glm::vec4 QuadVertices[] = { { -0.5f, -0.5f, 0.0f, 1.0f },
                                           { 0.5f, -0.5f, 0.0f, 1.0f },
                                           { 0.5f, 0.5f, 0.0f, 1.0f },
                                           { -0.5f, 0.5f, 0.0f, 1.0f } };

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

        glm::vec4 UnderlayColor;
        float UnderlayOffset;

        glm::vec4 OutlineColor;
        float OutlineThickness;

        int32_t ObjectId;
    };

    struct Renderer2DData
    {
        static const uint32_t MaxLines = 20000;
        static const uint32_t MaxLineVertices = MaxLines * 4;
        static const uint32_t MaxLineIndices = MaxLines * 6;

        // Quads
        Ref<VertexBuffer> QuadVertexBuffer;
        Ref<GraphicsPipeline> QuadPipeline;
        Ref<IndexBuffer> QuadIndexBuffer;
        Ref<UniformBufferBlock> QuadProjectionView;
        Ref<UniformParams> QuadUniforms;
        uint32_t QuadIndexCount = 0;
        uint32_t QuadVertexCount = 0;
        VertexData* QuadBuffer = nullptr;
        VertexData* QuadTmpBuffer = nullptr;

        // Circles
        Ref<VertexBuffer> CircleVertexBuffer;
        uint32_t CircleIndexCount = 0;
        uint32_t CircleVertexCount = 0;
        CircleVertex* CircleBuffer = nullptr;    // TODO: Better naming for these like base and current
        CircleVertex* CircleTmpBuffer = nullptr; // TODO: Better naming for these like base and current

        Ref<Material> CircleMaterial;
        Ref<UniformParams> CircleUniforms;

        // Text
        Ref<VertexBuffer> TextVertexBuffer;
        Ref<GraphicsPipeline> TextPipeline;
        uint32_t TextIndexCount = 0;
        uint32_t TextVertexCount = 0;
        TextVertex* TextBuffer = nullptr;
        TextVertex* TextTmpBuffer = nullptr;
        Ref<UniformBufferBlock> TextProjectionView;
        Ref<UniformParams> TextUniforms;

        // Font atlas only texture
        Ref<Texture> FontAtlasTexture;

        // Global texture buffer
        std::array<AssetHandle<Texture>, 32> Textures;
        uint32_t TextureIndex = 0;
    };

    static Renderer2DData* s_Data;

    static void SetupQuadBuffers()
    {
        uint16_t* indices = new uint16_t[RENDERER_INDICES_SIZE];
        int offset = 0;
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

        s_Data->QuadIndexBuffer = IndexBuffer::Create(indices, RENDERER_INDICES_SIZE);
        AssetHandle<Shader> shader = AssetManager::Get().Load<Shader>(RENDERER2D_SHADER_PATH);
        // Ref<Shader> shader = Importer::Get().Import<Shader>(RENDERER2D_SHADER_PATH);
        // Ref<ShaderStage> vertex = shader->GetStage(VERTEX_SHADER);
        // Ref<ShaderStage> fragment = shader->GetStage(FRAGMENT_SHADER);
        // s_Data->QuadVertexBuffer = VertexBuffer::Create(RENDERER_BUFFER_SIZE, BufferUsage::DYNAMIC_DRAW);
        // BufferLayout layout = { BufferElement(ShaderDataType::Float4, "a_Coordinates"),
        //                         BufferElement(ShaderDataType::Float4, "a_Color"),
        //                         BufferElement(ShaderDataType::Float2, "a_Uvs"),
        //                         BufferElement(ShaderDataType::Float, "a_Tid"),
        //                         BufferElement(ShaderDataType::Int, "a_ObjectID") };
        // s_Data->QuadVertexBuffer->SetLayout(layout);

        // PipelineStateDesc desc;
        // desc.FragmentShader = fragment;
        // desc.VertexShader = vertex;

        // s_Data->QuadPipeline = GraphicsPipeline::Create(desc, s_Data->QuadVertexBuffer->GetLayout());
        // s_Data->QuadProjectionView =
        //   UniformBufferBlock::Create(vertex->GetUniformDesc()->Uniforms.at("VP").BlockSize, BufferUsage::DYNAMIC_DRAW);
        // s_Data->QuadUniforms = UniformParams::Create(s_Data->QuadPipeline);
        // s_Data->QuadUniforms->SetUniformBlockBuffer(ShaderType::VERTEX_SHADER, "VP", s_Data->QuadProjectionView);
        // s_Data->QuadBuffer = s_Data->QuadTmpBuffer = new VertexData[RENDERER_MAX_SPRITES * 4];
        // delete[] indices;
    }

    static void SetupCircleBuffers()
    {
        s_Data->CircleBuffer = s_Data->CircleTmpBuffer = new CircleVertex[s_Data->MaxLineVertices];
        s_Data->CircleVertexBuffer = VertexBuffer::Create(s_Data->MaxLineVertices * sizeof(CircleVertex), BufferUsage::DYNAMIC_DRAW);
        const Ref<BufferLayout> layout = CreateRef<BufferLayout>(BufferLayout{
            { ShaderDataType::Float3, "a_WorldPosition" }, { ShaderDataType::Float3, "a_LocalPosition" },
            { ShaderDataType::Float4, "a_Color" },         { ShaderDataType::Float, "a_Thickness" },
            { ShaderDataType::Float, "a_Fade" },           { ShaderDataType::Int, "a_Id" }
        });
        s_Data->CircleVertexBuffer->SetLayout(layout);

        // AssetHandle<Shader> shader = AssetManager::Get().Load<Shader>("Resources/Shaders/Circle.asset");
        const Ref<Shader> circleShader = Importer::Get().Import<Shader>("Resources/Shaders/Circle.glsl");
        const Ref<Material> circleMaterial = Material::Create(circleShader);
        s_Data->CircleMaterial = circleMaterial;
        s_Data->CircleUniforms = circleMaterial->CreateUniformBuffer();
    }

    static void SetupTextBuffers()
    {
        /*
        s_Data->TextVertexBuffer =
          VertexBuffer::Create(RENDERER_MAX_SPRITES * sizeof(TextVertex), BufferUsage::DYNAMIC_DRAW);

        BufferLayout layout = {
            { ShaderDataType::Float3, "a_Position" },        { ShaderDataType::Float4, "a_Color" },
            { ShaderDataType::Float2, "a_TexCoords" },       { ShaderDataType::Float4, "a_UnderlayColor" },
            { ShaderDataType::Float, "a_UnderlayOffset" },   { ShaderDataType::Float4, "a_OutlineColor" },
            { ShaderDataType::Float, "a_OutlineThickness" }, { ShaderDataType::Int, "a_ObjectId" }
        };
        s_Data->TextVertexBuffer->SetLayout(layout);

        AssetHandle<Shader> shader = AssetManager::Get().Load<Shader>("Resources/Shaders/Text.asset");
        // Ref<Shader> shader = Importer::Get().Import<Shader>("Resources/Shaders/Text.glsl");
        // AssetManager::Get().Save(shader, "Resources/Shaders/Text.asset");
        Ref<ShaderStage> vertex = shader->GetStage(VERTEX_SHADER);
        Ref<ShaderStage> fragment = shader->GetStage(FRAGMENT_SHADER);
        PipelineStateDesc desc;
        desc.FragmentShader = fragment;
        desc.VertexShader = vertex;

        s_Data->TextPipeline = GraphicsPipeline::Create(desc, layout);
        s_Data->TextBuffer = s_Data->TextTmpBuffer = new TextVertex[s_Data->MaxLineVertices];
        // Is this necessary? And when did I write it
        s_Data->TextProjectionView = UniformBufferBlock::Create(
          vertex->GetUniformDesc()->Uniforms.at("Camera").BlockSize, BufferUsage::DYNAMIC_DRAW);
        s_Data->TextUniforms = UniformParams::Create(s_Data->TextPipeline);
        s_Data->TextUniforms->SetUniformBlockBuffer(ShaderType::VERTEX_SHADER, "Camera", s_Data->TextProjectionView);
        */
    }

    void Renderer2D::Init()
    {
        s_Data = new Renderer2DData();
        // s_Data->Textures[0] = Texture::WHITE;
        SetupQuadBuffers();
        SetupCircleBuffers();
        SetupTextBuffers();
    }

    void Renderer2D::Begin(const Camera& camera, const glm::mat4& viewMatrix)
    {
        CW_ENGINE_ASSERT(false);
        const glm::mat4 viewProjection = camera.GetProjection() * viewMatrix;
    }

    void Renderer2D::Begin(const glm::mat4& projection, const glm::mat4& view)
    {
        CW_ENGINE_ASSERT(false);
    }

    float Renderer2D::FindTexture(const AssetHandle<Texture>& texture)
    {
        if (!texture)
            return 0;

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
                s_Data->QuadBuffer = (VertexData*)s_Data->QuadVertexBuffer->Map(
                  0, RENDERER_MAX_SPRITES * 4,
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
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), { bounds.X, bounds.Y, 1.0f }) *
                              glm::scale(glm::mat4(1.0f), { bounds.Width, bounds.Height, 1.0f });

        FillRect(transform, nullptr, color, entityId);
    }

    void Renderer2D::FillRect(const glm::mat4& transform, const AssetHandle<Texture>& texture, const glm::vec4& color,
                              uint32_t entityId)
    {
        float ts = FindTexture(texture);

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

    void Renderer2D::FillRect(const Rect2F& bounds, const AssetHandle<Texture>& texture, const glm::vec4& color,
                              uint32_t entityId)
    {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), { bounds.X, bounds.Y, 1.0f }) *
                              glm::scale(glm::mat4(1.0f), { bounds.Width, bounds.Height, 1.0f });

        FillRect(transform, texture, color, entityId);
    }

    void Renderer2D::DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness, float fade,
                                int32_t entityId)
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
        float length = glm::length(p1 - p2);
        glm::vec3 center = (p1 + p2) * 0.5f;
        float angle = glm::atan(glm::abs(p1.y - p2.y) / glm::abs(p1.x - p2.x));
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), center) *
                              glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 0.0f, 1.0f)) *
                              glm::scale(glm::mat4(1.0f), glm::vec3(length, thickness, 1.0f));
        FillRect(transform, nullptr, color, 0);
    }

    void Renderer2D::DrawRect(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color, float thickness)
    {
        glm::vec3 p0 = glm::vec3(position.x - size.x * 0.5f, position.y - size.y * 0.5f, position.z);
        glm::vec3 p1 = glm::vec3(position.x + size.x * 0.5f, position.y - size.y * 0.5f, position.z);
        glm::vec3 p2 = glm::vec3(position.x + size.x * 0.5f, position.y + size.y * 0.5f, position.z);
        glm::vec3 p3 = glm::vec3(position.x - size.x * 0.5f, position.y + size.y * 0.5f, position.z);

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
        if (!textComponent.Font) // TODO: Use default font here.
            return;

        const msdf_atlas::FontGeometry& fontGeometry = textComponent.Font->GetMSDFData()->FontGeometry;
        const msdfgen::FontMetrics& fontMetrics = fontGeometry.getMetrics();

        // TODO: Make this use an array for font textures or reset when the texture is different.
        Ref<Texture> fontAtlasTexture = textComponent.Font->GetAtlasTexture();
        s_Data->FontAtlasTexture = fontAtlasTexture;

        double x = 0.0;
        double fsScale = 1.0 / (fontMetrics.ascenderY - fontMetrics.descenderY);
        double y = 0;

        const float spaceGlyphAdvance = (float)fontGeometry.getGlyph(' ')->getAdvance();

        const String& text = textComponent.Text;
        for (uint32_t i = 0; i < (uint32_t)text.size(); i++)
        {
            char character = text[i];
            if (character == '\r')
                continue;

            if (character == '\n')
            {
                x = 0;
                y -= fsScale * fontMetrics.lineHeight + textComponent.LineSpacing;
                continue;
            }

            if (character == ' ')
            {
                float advance = spaceGlyphAdvance;
                if (i < text.size() - 1)
                {
                    char nextCharacter = text[i + 1];
                    double fontKerningAdvance = 0.0;
                    if (textComponent.UseKerning)
                        fontGeometry.getAdvance(fontKerningAdvance, character, nextCharacter);
                    advance = (float)fontKerningAdvance + textComponent.WordSpacing;
                }

                x += fsScale * advance;
                continue;
            }

            if (character == '\t')
                x += 4.0f * (fsScale * spaceGlyphAdvance + textComponent.CharacterSpacing);

            const msdf_atlas::GlyphGeometry* glyphGeometry = fontGeometry.getGlyph(character);
            if (!glyphGeometry)
                glyphGeometry = fontGeometry.getGlyph('?'); // TODO: Add missing symbol setting
            if (!glyphGeometry)
                return;

            double al, ab, ar, at;
            glyphGeometry->getQuadAtlasBounds(al, ab, ar, at);
            glm::vec2 texCoordMin((float)al, (float)ab);
            glm::vec2 texCoordMax((float)ar, (float)at);

            double pl, pb, pr, pt;
            glyphGeometry->getQuadPlaneBounds(pl, pb, pr, pt);
            glm::vec2 quadMin((float)pl, (float)pb);
            glm::vec2 quadMax((float)pr, (float)pt);

            quadMin *= fsScale;
            quadMax *= fsScale;
            quadMin += glm::vec2(x, y);
            quadMax += glm::vec2(x, y);

            float texelWidth = 1.0f / fontAtlasTexture->GetWidth();
            float texelHeight = 1.0f / fontAtlasTexture->GetHeight();
            texCoordMin *= glm::vec2(texelWidth, texelHeight);
            texCoordMax *= glm::vec2(texelWidth, texelHeight);

            // TODO: Check if get kerning data is enabled in the font asset.
            double advance = glyphGeometry->getAdvance();
            if (textComponent.UseKerning && i < text.size() - 1)
            {
                char nextCharacter = text[i + 1];
                fontGeometry.getAdvance(advance, character, nextCharacter);
            }
            x += fsScale * advance + textComponent.CharacterSpacing;

            s_Data->TextBuffer->Position = transform * glm::vec4(quadMin, 0.0f, 1.0f);
            s_Data->TextBuffer->Color = textComponent.Color;
            s_Data->TextBuffer->TexCoords = texCoordMin;
            s_Data->TextBuffer->ObjectId = entityId;
            s_Data->TextBuffer++;

            s_Data->TextBuffer->Position = transform * glm::vec4(quadMin.x, quadMax.y, 0.0f, 1.0f);
            s_Data->TextBuffer->Color = textComponent.Color;
            s_Data->TextBuffer->TexCoords = { texCoordMin.x, texCoordMax.y };
            s_Data->TextBuffer->ObjectId = entityId;
            s_Data->TextBuffer++;

            s_Data->TextBuffer->Position = transform * glm::vec4(quadMax, 0.0f, 1.0f);
            s_Data->TextBuffer->Color = textComponent.Color;
            s_Data->TextBuffer->TexCoords = texCoordMax;
            s_Data->TextBuffer->ObjectId = entityId;
            s_Data->TextBuffer++;

            s_Data->TextBuffer->Position = transform * glm::vec4(quadMax.x, quadMin.y, 0.0f, 1.0f);
            s_Data->TextBuffer->Color = textComponent.Color;
            s_Data->TextBuffer->TexCoords = { texCoordMax.x, texCoordMin.y };
            s_Data->TextBuffer->ObjectId = entityId;
            s_Data->TextBuffer++;

            s_Data->TextIndexCount += 6;
            s_Data->TextVertexCount += 4;
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

        s_Data->TextBuffer = s_Data->TextTmpBuffer;
        s_Data->TextIndexCount = 0;
        s_Data->TextVertexCount = 0;
    }

    static void FlushQuads()
    {
        if (s_Data->QuadIndexCount > 0)
        {
            RenderAPI::Get().SetGraphicsPipeline(s_Data->QuadPipeline);
            RenderAPI::Get().SetVertexBuffers(0, &s_Data->QuadVertexBuffer, 1);
            RenderAPI::Get().SetIndexBuffer(s_Data->QuadIndexBuffer);
            void* data = s_Data->QuadVertexBuffer->Map(0, s_Data->QuadVertexCount * sizeof(VertexData), GpuLockOptions::WRITE_DISCARD);
            std::memcpy(data, s_Data->QuadTmpBuffer, s_Data->QuadVertexCount * sizeof(VertexData));
            s_Data->QuadVertexBuffer->Unmap();
            for (uint32_t i = 0; i < 8; i++)
            {
                if (s_Data->Textures[i])
                    s_Data->QuadUniforms->SetTexture(0, 1 + i, s_Data->Textures[i].GetInternalPtr());
                else
                    s_Data->QuadUniforms->SetTexture(0, 1 + i, s_Data->Textures[0].GetInternalPtr());
            }
            RenderAPI::Get().SetUniforms(s_Data->QuadUniforms);
            RenderAPI::Get().DrawIndexed(0, s_Data->QuadIndexCount, 0, s_Data->QuadVertexCount);
        }
    }

    static void FlushCircles()
    {
        if (s_Data->CircleIndexCount > 0)
        {
            RenderAPI::Get().SetGraphicsPipeline(s_Data->CircleMaterial->GetPass()->GetGraphicsPipeline());
            RenderAPI::Get().SetVertexBuffers(0, &s_Data->CircleVertexBuffer, 1);
            // TODO: Replace with WriteData.
            void* data = s_Data->CircleVertexBuffer->Map(0, s_Data->CircleVertexCount * sizeof(CircleVertex), GpuLockOptions::WRITE_DISCARD);
            std::memcpy(data, s_Data->CircleTmpBuffer, s_Data->CircleVertexCount * sizeof(CircleVertex));
            s_Data->CircleVertexBuffer->Unmap();
            RenderAPI::Get().SetUniforms(s_Data->CircleUniforms);
            RenderAPI::Get().DrawIndexed(0, s_Data->CircleIndexCount, 0, s_Data->CircleVertexCount);
        }
    }

    static void FlushText()
    {
        if (s_Data->TextIndexCount > 0)
        {
            RenderAPI::Get().SetGraphicsPipeline(s_Data->TextPipeline);
            RenderAPI::Get().SetVertexBuffers(0, &s_Data->TextVertexBuffer, 1);
            RenderAPI::Get().SetIndexBuffer(s_Data->QuadIndexBuffer);
            void* data = s_Data->TextVertexBuffer->Map(0, s_Data->TextVertexCount * sizeof(TextVertex), GpuLockOptions::WRITE_DISCARD);
            std::memcpy(data, s_Data->TextTmpBuffer, s_Data->TextVertexCount * sizeof(TextVertex));
            s_Data->TextVertexBuffer->Unmap();
            s_Data->TextUniforms->SetTexture(0, 1, s_Data->FontAtlasTexture);
            RenderAPI::Get().SetUniforms(s_Data->TextUniforms);
            RenderAPI::Get().DrawIndexed(0, s_Data->TextIndexCount, 0, s_Data->TextVertexCount);
        }
    }

    void Renderer2D::Flush()
    {
        // FlushQuads();
        FlushCircles();
        // FlushText();
    }

    void Renderer2D::Shutdown()
    {
        s_Data->QuadIndexBuffer = nullptr;
        s_Data->QuadVertexBuffer = nullptr;
        s_Data->QuadPipeline = nullptr;
        s_Data->QuadUniforms = nullptr;
        s_Data->CircleVertexBuffer = nullptr;
        s_Data->CircleMaterial = nullptr;
        s_Data->TextVertexBuffer = nullptr;
        s_Data->TextPipeline = nullptr;
        s_Data->FontAtlasTexture = nullptr;

        for (uint32_t i = 0; i < 8; i++)
            s_Data->Textures[i] = nullptr;
        delete[] s_Data->QuadTmpBuffer;
        delete s_Data;
    }
} // namespace Crowny

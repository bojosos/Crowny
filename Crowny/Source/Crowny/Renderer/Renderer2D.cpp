#include "cwpch.h"

#include "Crowny/Renderer/Renderer2D.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/RenderAPI/GraphicsPipeline.h"
#include "Crowny/RenderAPI/RenderCommand.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/UniformParams.h"
#include "Crowny/RenderAPI/VertexArray.h"
#include "Crowny/Renderer/Camera.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Utils/ShaderCompiler.h"

#include <glm/ext/matrix_transform.hpp>
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
        float Tid;
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

    struct Renderer2DData
    {
        static const uint32_t MaxLines = 20000;
        static const uint32_t MaxLineVertices = MaxLines * 4;
        static const uint32_t MaxLineIndices = MaxLines * 6;

        Ref<VertexBuffer> QuadVertexBuffer;
        Ref<GraphicsPipeline> QuadPipeline;
        Ref<IndexBuffer> QuadIndexBuffer;
        Ref<UniformBufferBlock> QuadProjectionView;
        Ref<UniformParams> QuadUniforms;
        uint32_t QuadIndexCount = 0;
        uint32_t QuadVertexCount = 0;
        VertexData* QuadBuffer = nullptr;
        VertexData* QuadTmpBuffer = nullptr;

        Ref<VertexBuffer> CircleVertexBuffer;
        Ref<GraphicsPipeline> CirclePipeline;
        uint32_t CircleIndexCount = 0;
        uint32_t CircleVertexCount = 0;
        CircleVertex* CircleBuffer = nullptr;
        CircleVertex* CircleTmpBuffer = nullptr;
        Ref<UniformBufferBlock> CircleProjectionView;
        Ref<UniformParams> CircleUniforms;

        std::array<Ref<Texture>, 32> Textures;
        uint32_t TextureIndex = 0;
    };

    static Renderer2DData* s_Data;

    void Renderer2D::Init()
    {
        s_Data = new Renderer2DData();
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
            Ref<ShaderStage> vertex = shader->GetStage(VERTEX_SHADER);
            Ref<ShaderStage> fragment = shader->GetStage(FRAGMENT_SHADER);
            s_Data->QuadVertexBuffer = VertexBuffer::Create(RENDERER_BUFFER_SIZE, BufferUsage::DYNAMIC_DRAW);
            BufferLayout layout = { { ShaderDataType::Float4, "a_Coordinates" },
                                    { ShaderDataType::Float4, "a_Color" },
                                    { ShaderDataType::Float2, "a_Uvs" },
                                    { ShaderDataType::Float, "a_Tid" },
                                    { ShaderDataType::Int, "a_ObjectID" } };
            s_Data->QuadVertexBuffer->SetLayout(layout);

            PipelineStateDesc desc;
            desc.FragmentShader = fragment;
            desc.VertexShader = vertex;

            s_Data->QuadPipeline = GraphicsPipeline::Create(desc, s_Data->QuadVertexBuffer->GetLayout());
            s_Data->QuadProjectionView = UniformBufferBlock::Create(
              vertex->GetUniformDesc()->Uniforms.at("VP").BlockSize, BufferUsage::DYNAMIC_DRAW);
            s_Data->QuadUniforms = UniformParams::Create(s_Data->QuadPipeline);
            s_Data->QuadUniforms->SetUniformBlockBuffer(ShaderType::VERTEX_SHADER, "VP", s_Data->QuadProjectionView);
            s_Data->QuadBuffer = s_Data->QuadTmpBuffer = new VertexData[RENDERER_MAX_SPRITES * 4];
            delete[] indices;
        }
        {
            s_Data->CircleVertexBuffer =
              VertexBuffer::Create(s_Data->MaxLineVertices * sizeof(CircleVertex), BufferUsage::DYNAMIC_DRAW);
            BufferLayout layout = {
                { ShaderDataType::Float3, "a_WorldPosition" }, { ShaderDataType::Float3, "a_LocalPosition" },
                { ShaderDataType::Float4, "a_Color" },         { ShaderDataType::Float, "a_Thickness" },
                { ShaderDataType::Float, "a_Fade" },           { ShaderDataType::Int, "a_Id" }
            };
            s_Data->CircleVertexBuffer->SetLayout(layout);

            AssetHandle<Shader> shader = AssetManager::Get().Load<Shader>("Resources/Shaders/Circle.asset");
            // Ref<Shader> shader = Importer::Get().Import<Shader>("Resources/Shaders/Circle.glsl");
            Ref<ShaderStage> vertex = shader->GetStage(VERTEX_SHADER);
            Ref<ShaderStage> fragment = shader->GetStage(FRAGMENT_SHADER);
            PipelineStateDesc desc;
            desc.FragmentShader = fragment;
            desc.VertexShader = vertex;

            s_Data->CirclePipeline = GraphicsPipeline::Create(desc, layout);
            s_Data->CircleBuffer = s_Data->CircleTmpBuffer = new CircleVertex[s_Data->MaxLineVertices];
            s_Data->CircleProjectionView = UniformBufferBlock::Create(
              vertex->GetUniformDesc()->Uniforms.at("Camera").BlockSize, BufferUsage::DYNAMIC_DRAW);
            s_Data->CircleUniforms = UniformParams::Create(s_Data->CirclePipeline);
            s_Data->CircleUniforms->SetUniformBlockBuffer(ShaderType::VERTEX_SHADER, "Camera",
                                                          s_Data->CircleProjectionView);
        }
        TextureParameters params;
        params.Width = 1;
        params.Height = 1;
        params.Shape = TextureShape::TEXTURE_2D;
        params.Format = TextureFormat::RGBA8;

        s_Data->Textures[0] = Texture::WHITE;
    }

    void Renderer2D::Begin(const Camera& camera, const glm::mat4& viewMatrix)
    {
        s_Data->QuadProjectionView->Write(0, glm::value_ptr(viewMatrix), sizeof(glm::mat4));
        s_Data->QuadProjectionView->Write(sizeof(glm::mat4), glm::value_ptr(camera.GetProjection()), sizeof(glm::mat4));
        glm::mat4 vp = camera.GetProjection() * viewMatrix;
        s_Data->CircleProjectionView->Write(0, glm::value_ptr(vp), sizeof(glm::mat4));
    }

    void Renderer2D::Begin(const glm::mat4& projection, const glm::mat4& view)
    {
        s_Data->QuadProjectionView->Write(0, glm::value_ptr(view), sizeof(glm::mat4));
        s_Data->QuadProjectionView->Write(sizeof(glm::mat4), glm::value_ptr(projection), sizeof(glm::mat4));
        glm::mat4 vp = projection * view;
        s_Data->CircleProjectionView->Write(0, glm::value_ptr(vp), sizeof(glm::mat4));
    }

    float Renderer2D::FindTexture(const Ref<Texture>& texture)
    {
        if (!texture)
            return 0;

        float ts = 0.0f;

        for (uint8_t i = 1; i <= s_Data->TextureIndex; i++)
        {
            if (s_Data->Textures[i] == texture)
            {
                ts = (float)(i + 1);
                break;
            }
        }

        if (ts == 0)
        {
            if (s_Data->TextureIndex == 32) // TODO: not 32 please
            {
                End();
                s_Data->QuadBuffer = (VertexData*)s_Data->QuadVertexBuffer->Map(
                  0, RENDERER_MAX_SPRITES * 4,
                  GpuLockOptions::WRITE_DISCARD); // TODO: Begin or semething instead of this
            }
            s_Data->Textures[++s_Data->TextureIndex] = texture;
            ts = (float)s_Data->TextureIndex;
        }
        return ts;
    }

    void Renderer2D::FillRect(const Rect2F& bounds, const glm::vec4& color, uint32_t entityId)
    {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), { bounds.X, bounds.Y, 1.0f }) *
                              glm::scale(glm::mat4(1.0f), { bounds.Width, bounds.Height, 1.0f });

        FillRect(transform, nullptr, color, entityId);
    }

    void Renderer2D::FillRect(const glm::mat4& transform, const Ref<Texture>& texture, const glm::vec4& color,
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

    void Renderer2D::FillRect(const Rect2F& bounds, const Ref<Texture>& texture, const glm::vec4& color,
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

    void Renderer2D::DrawString(const String& text, float x, float y, const Ref<Font>& font, const glm::vec4& color)
    {
        // float ts = FindTexture(font->GetTexture());
        /*
        texture_font_t* ftFont = font->GetFTGLFont();

        for (uint32_t i = 0; i < text.length(); i++)
        {
            char c = text[i];
            texture_glyph_t* glyph = texture_font_get_glyph(ftFont, &c);

            if (glyph)
            {
                if (i > 0)
                {
                    float kerning = texture_glyph_get_kerning(glyph, &text[i - 1]);
                    x += kerning;
                }

                float x0 = x + glyph->offset_x;
                float y0 = y + glyph->offset_y;
                float x1 = x0 + glyph->width;
                float y1 = y0 - glyph->height;

                float u0 = glyph->s0;
                float v0 = glyph->t0;
                float u1 = glyph->s1;
                float v1 = glyph->t1;

                s_Data->QuadBuffer->Position = glm::vec4(x0, y0, 0, 1.0f);
                s_Data->QuadBuffer->Uv = glm::vec2(u0, v0);
                s_Data->QuadBuffer->Tid = ts;
                s_Data->QuadBuffer->Color = color;
                s_Data->QuadBuffer++;

                s_Data->QuadBuffer->Position = glm::vec4(x0, y1, 0, 1.0f);
                s_Data->QuadBuffer->Uv = glm::vec2(u0, v1);
                s_Data->QuadBuffer->Tid = ts;
                s_Data->QuadBuffer->Color = color;
                s_Data->QuadBuffer++;

                s_Data->QuadBuffer->Position = glm::vec4(x1, y1, 0, 1.0f);
                s_Data->QuadBuffer->Uv = glm::vec2(u1, v1);
                s_Data->QuadBuffer->Tid = ts;
                s_Data->QuadBuffer->Color = color;
                s_Data->QuadBuffer++;

                s_Data->QuadBuffer->Position = glm::vec4(x1, y0, 0, 1.0f);
                s_Data->QuadBuffer->Uv = glm::vec2(u1, v0);
                s_Data->QuadBuffer->Tid = ts;
                s_Data->QuadBuffer->Color = color;
                s_Data->QuadBuffer++;

                s_Data->QuadVertexCount += 4;
                s_Data->QuadIndexCount += 6;

                x += glyph->advance_x;
            }
        }*/
    }

    void Renderer2D::DrawString(const String& text, const glm::mat4& transform, const Ref<Font>& font,
                                const glm::vec4& color)
    {
        float x = transform[3][0];
        float y = transform[3][1];
        // float ts = FindTexture(font->GetTexture());
        /*
        texture_font_t* ftFont = font->GetFTGLFont();

        for (uint32_t i = 0; i < text.length(); i++)
        {
            char c = text[i];
            texture_glyph_t* glyph = texture_font_get_glyph(ftFont, &c);

            if (glyph)
            {
                if (i > 0)
                {
                    float kerning = texture_glyph_get_kerning(glyph, &text[i - 1]);
                    x += kerning;
                }

                float x0 = x + glyph->offset_x;
                float y0 = y + glyph->offset_y;
                float x1 = x0 + glyph->width;
                float y1 = y0 - glyph->height;

                float u0 = glyph->s0;
                float v0 = glyph->t0;
                float u1 = glyph->s1;
                float v1 = glyph->t1;

                s_Data->QuadBuffer->Position = transform * glm::vec4(x0, y0, 0, 1.0f);
                s_Data->QuadBuffer->Uv = glm::vec2(u0, v0);
                s_Data->QuadBuffer->Tid = ts;
                s_Data->QuadBuffer->Color = color;
                s_Data->QuadBuffer++;

                s_Data->QuadBuffer->Position = transform * glm::vec4(x0, y1, 0, 1.0f);
                s_Data->QuadBuffer->Uv = glm::vec2(u0, v1);
                s_Data->QuadBuffer->Tid = ts;
                s_Data->QuadBuffer->Color = color;
                s_Data->QuadBuffer++;

                s_Data->QuadBuffer->Position = transform * glm::vec4(x1, y1, 0, 1.0f);
                s_Data->QuadBuffer->Uv = glm::vec2(u1, v1);
                s_Data->QuadBuffer->Tid = ts;
                s_Data->QuadBuffer->Color = color;
                s_Data->QuadBuffer++;

                s_Data->QuadBuffer->Position = transform * glm::vec4(x1, y0, 0, 1.0f);
                s_Data->QuadBuffer->Uv = glm::vec2(u1, v0);
                s_Data->QuadBuffer->Tid = ts;
                s_Data->QuadBuffer->Color = color;
                s_Data->QuadBuffer++;

                s_Data->QuadVertexCount += 4;
                s_Data->QuadIndexCount += 6;

                x += glyph->advance_x;
            }
        }*/
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
    }

    void Renderer2D::Flush()
    {
        if (s_Data->QuadIndexCount > 0)
        {
            RenderAPI::Get().SetGraphicsPipeline(s_Data->QuadPipeline);
            RenderAPI::Get().SetVertexBuffers(0, &s_Data->QuadVertexBuffer, 1);
            RenderAPI::Get().SetIndexBuffer(s_Data->QuadIndexBuffer);
            void* data = s_Data->QuadVertexBuffer->Map(0, s_Data->QuadVertexCount * sizeof(VertexData),
                                                       GpuLockOptions::WRITE_DISCARD);
            std::memcpy(data, s_Data->QuadTmpBuffer, s_Data->QuadVertexCount * sizeof(VertexData));
            s_Data->QuadVertexBuffer->Unmap();
            for (uint32_t i = 0; i < 8; i++)
                if (s_Data->Textures[i])
                    s_Data->QuadUniforms->SetTexture(0, 1 + i, s_Data->Textures[i]);
                else
                    s_Data->QuadUniforms->SetTexture(0, 1 + i, s_Data->Textures[0]);
            RenderAPI::Get().SetUniforms(s_Data->QuadUniforms);
            RenderAPI::Get().DrawIndexed(0, s_Data->QuadIndexCount, 0, s_Data->QuadVertexCount);
        }
        if (s_Data->CircleIndexCount > 0)
        {
            RenderAPI::Get().SetGraphicsPipeline(s_Data->CirclePipeline);
            RenderAPI::Get().SetVertexBuffers(0, &s_Data->CircleVertexBuffer, 1);
            RenderAPI::Get().SetIndexBuffer(s_Data->QuadIndexBuffer);
            void* data = s_Data->CircleVertexBuffer->Map(0, s_Data->CircleVertexCount * sizeof(CircleVertex),
                                                         GpuLockOptions::WRITE_DISCARD);
            std::memcpy(data, s_Data->CircleTmpBuffer, s_Data->CircleVertexCount * sizeof(CircleVertex));
            s_Data->CircleVertexBuffer->Unmap();
            RenderAPI::Get().SetUniforms(s_Data->CircleUniforms);
            RenderAPI::Get().DrawIndexed(0, s_Data->CircleIndexCount, 0, s_Data->CircleVertexCount);
        }
    }

    void Renderer2D::Shutdown()
    {
        s_Data->QuadIndexBuffer = nullptr;
        s_Data->QuadVertexBuffer = nullptr;
        s_Data->QuadPipeline = nullptr;
        s_Data->QuadUniforms = nullptr;
        s_Data->CircleVertexBuffer = nullptr;
        s_Data->CirclePipeline = nullptr;

        for (uint32_t i = 0; i < 8; i++)
            s_Data->Textures[i] = nullptr;
        delete[] s_Data->QuadTmpBuffer;
        delete s_Data;
    }
} // namespace Crowny

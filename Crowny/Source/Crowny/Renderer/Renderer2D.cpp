#include "cwpch.h"

#include "Crowny/Renderer/Renderer2D.h"

#include "Crowny/RenderAPI/GraphicsPipeline.h"
#include "Crowny/RenderAPI/RenderCommand.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Utils/ShaderCompiler.h"

#include <freetype-gl.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Crowny
{

    constexpr glm::vec2 QuadUv[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
    constexpr glm::vec4 QuadVertices[] = { { -0.5f, -0.5f, 0.0f, 1.0f },
                                           { 0.5f, -0.5f, 0.0f, 1.0f },
                                           { 0.5f, 0.5f, 0.0f, 1.0f },
                                           { -0.5f, 0.5f, 0.0f, 1.0f } };

    struct Renderer2DData
    {
        Ref<VertexBuffer> VertexBuffer;
        Ref<GraphicsPipeline> Pipeline;
        Ref<UniformBufferBlock> ProjectionView;
        Ref<UniformParams> Uniforms;
        Ref<IndexBuffer> IndexBuffer;

        uint32_t IndexCount = 0;
        uint32_t VertexCount = 0;
        VertexData* Buffer = nullptr;
        VertexData* TmpBuffer = nullptr;

        std::array<Ref<Texture>, 32> Textures;
        uint32_t TextureIndex = 0;
    };

    static Renderer2DData* s_Data;

    void Renderer2D::Init()
    {
        s_Data = new Renderer2DData();
        uint16_t* indices = new uint16_t[RENDERER_INDICES_SIZE];
        int offset = 0;
        for (int i = 0; i < RENDERER_INDICES_SIZE; i += 6)
        {
            indices[i] = offset + 0;
            indices[i + 1] = offset + 1;
            indices[i + 2] = offset + 2;

            indices[i + 3] = offset + 2;
            indices[i + 4] = offset + 3;
            indices[i + 5] = offset + 0;

            offset += 4;
        }

        s_Data->IndexBuffer = IndexBuffer::Create(indices, RENDERER_INDICES_SIZE);
        ShaderCompiler compiler;
        Ref<Shader> vertex = Shader::Create(compiler.Compile("/Shaders/BatchRenderer.vert", VERTEX_SHADER));
        Ref<Shader> fragment = Shader::Create(compiler.Compile("/Shaders/BatchRenderer.frag", FRAGMENT_SHADER));
        s_Data->VertexBuffer = VertexBuffer::Create(RENDERER_BUFFER_SIZE, BufferUsage::DYNAMIC_DRAW);
        BufferLayout layout = { { ShaderDataType::Float4, "a_Coordinates" },
                                { ShaderDataType::Float4, "a_Color" },
                                { ShaderDataType::Float2, "a_Uvs" },
                                { ShaderDataType::Float, "a_Tid" },
                                { ShaderDataType::Int, "a_ObjectID" } };
        s_Data->VertexBuffer->SetLayout(layout);

        PipelineStateDesc desc;
        desc.FragmentShader = fragment;
        desc.VertexShader = vertex;

        s_Data->Pipeline = GraphicsPipeline::Create(desc, s_Data->VertexBuffer->GetLayout());
        s_Data->ProjectionView =
          UniformBufferBlock::Create(vertex->GetUniformDesc()->Uniforms.at("VP").BlockSize, BufferUsage::DYNAMIC_DRAW);
        s_Data->Uniforms = UniformParams::Create(s_Data->Pipeline);
        s_Data->Uniforms->SetUniformBlockBuffer(ShaderType::VERTEX_SHADER, "VP", s_Data->ProjectionView);

        TextureParameters params;
        params.Width = 1;
        params.Height = 1;
        params.Shape = TextureShape::TEXTURE_2D;
        params.Format = TextureFormat::RGBA8;

        s_Data->Textures[0] = Texture::WHITE;
        s_Data->Buffer = s_Data->TmpBuffer = new VertexData[RENDERER_SPRITE_SIZE];
        delete[] indices;
    }

    void Renderer2D::Begin(const Camera& camera, const glm::mat4& viewMatrix)
    {
        s_Data->ProjectionView->Write(0, glm::value_ptr(viewMatrix), sizeof(glm::mat4));
        s_Data->ProjectionView->Write(sizeof(glm::mat4), glm::value_ptr(camera.GetProjection()), sizeof(glm::mat4));

        RenderAPI::Get().SetGraphicsPipeline(s_Data->Pipeline);
        RenderAPI::Get().SetVertexBuffers(0, &s_Data->VertexBuffer, 1);
        RenderAPI::Get().SetIndexBuffer(s_Data->IndexBuffer);
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
                s_Data->Buffer = (VertexData*)s_Data->VertexBuffer->Map(
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
            s_Data->Buffer->Position = transform * QuadVertices[i];
            s_Data->Buffer->Uv = QuadUv[i];
            s_Data->Buffer->Tid = ts;
            s_Data->Buffer->Color = color;
            s_Data->Buffer->ObjectID = entityId;
            s_Data->Buffer++;
        }

        s_Data->VertexCount += 4;
        s_Data->IndexCount += 6;
    }

    void Renderer2D::FillRect(const Rect2F& bounds, const Ref<Texture>& texture, const glm::vec4& color,
                              uint32_t entityId)
    {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), { bounds.X, bounds.Y, 1.0f }) *
                              glm::scale(glm::mat4(1.0f), { bounds.Width, bounds.Height, 1.0f });

        FillRect(transform, texture, color, entityId);
    }

    void Renderer2D::DrawString(const String& text, float x, float y, const Ref<Font>& font, const glm::vec4& color)
    {
        float ts = FindTexture(font->GetTexture());

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

                s_Data->Buffer->Position = glm::vec4(x0, y0, 0, 1.0f);
                s_Data->Buffer->Uv = glm::vec2(u0, v0);
                s_Data->Buffer->Tid = ts;
                s_Data->Buffer->Color = color;
                s_Data->Buffer++;

                s_Data->Buffer->Position = glm::vec4(x0, y1, 0, 1.0f);
                s_Data->Buffer->Uv = glm::vec2(u0, v1);
                s_Data->Buffer->Tid = ts;
                s_Data->Buffer->Color = color;
                s_Data->Buffer++;

                s_Data->Buffer->Position = glm::vec4(x1, y1, 0, 1.0f);
                s_Data->Buffer->Uv = glm::vec2(u1, v1);
                s_Data->Buffer->Tid = ts;
                s_Data->Buffer->Color = color;
                s_Data->Buffer++;

                s_Data->Buffer->Position = glm::vec4(x1, y0, 0, 1.0f);
                s_Data->Buffer->Uv = glm::vec2(u1, v0);
                s_Data->Buffer->Tid = ts;
                s_Data->Buffer->Color = color;
                s_Data->Buffer++;

                s_Data->VertexCount += 4;
                s_Data->IndexCount += 6;

                x += glyph->advance_x;
            }
        }
    }

    void Renderer2D::DrawString(const String& text, const glm::mat4& transform, const Ref<Font>& font,
                                const glm::vec4& color)
    {
        float x = transform[3][0];
        float y = transform[3][1];
        float ts = FindTexture(font->GetTexture());

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

                s_Data->Buffer->Position = transform * glm::vec4(x0, y0, 0, 1.0f);
                s_Data->Buffer->Uv = glm::vec2(u0, v0);
                s_Data->Buffer->Tid = ts;
                s_Data->Buffer->Color = color;
                s_Data->Buffer++;

                s_Data->Buffer->Position = transform * glm::vec4(x0, y1, 0, 1.0f);
                s_Data->Buffer->Uv = glm::vec2(u0, v1);
                s_Data->Buffer->Tid = ts;
                s_Data->Buffer->Color = color;
                s_Data->Buffer++;

                s_Data->Buffer->Position = transform * glm::vec4(x1, y1, 0, 1.0f);
                s_Data->Buffer->Uv = glm::vec2(u1, v1);
                s_Data->Buffer->Tid = ts;
                s_Data->Buffer->Color = color;
                s_Data->Buffer++;

                s_Data->Buffer->Position = transform * glm::vec4(x1, y0, 0, 1.0f);
                s_Data->Buffer->Uv = glm::vec2(u1, v0);
                s_Data->Buffer->Tid = ts;
                s_Data->Buffer->Color = color;
                s_Data->Buffer++;

                s_Data->VertexCount += 4;
                s_Data->IndexCount += 6;

                x += glyph->advance_x;
            }
        }
    }

    void Renderer2D::End()
    {
        void* data =
          s_Data->VertexBuffer->Map(0, s_Data->VertexCount * sizeof(VertexData), GpuLockOptions::WRITE_DISCARD);
        std::memcpy(data, s_Data->TmpBuffer, s_Data->VertexCount * sizeof(VertexData));
        s_Data->VertexBuffer->Unmap();
        Flush();
        s_Data->Buffer = s_Data->TmpBuffer;
        s_Data->IndexCount = 0;
        s_Data->VertexCount = 0;
        s_Data->TextureIndex = 0;
    }

    void Renderer2D::Flush()
    {
        for (uint32_t i = 0; i < 8; i++)
            if (s_Data->Textures[i])
                s_Data->Uniforms->SetTexture(0, 1 + i, s_Data->Textures[i]);
            else
                s_Data->Uniforms->SetTexture(0, 1 + i, s_Data->Textures[0]);
        RenderAPI::Get().SetUniforms(s_Data->Uniforms);
        RenderAPI::Get().DrawIndexed(0, s_Data->IndexCount, 0, s_Data->VertexCount);
    }

    void Renderer2D::Shutdown()
    {
        s_Data->IndexBuffer = nullptr;
        s_Data->VertexBuffer = nullptr;
        s_Data->Pipeline = nullptr;
        s_Data->Uniforms = nullptr;
        for (uint32_t i = 0; i < 8; i++)
            s_Data->Textures[i] = nullptr;
        delete s_Data;
    }
} // namespace Crowny

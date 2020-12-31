#include "cwpch.h"

#include "Crowny/Renderer/Shader.h"
#include "Crowny/Renderer/Texture.h"
#include "Crowny/Renderer/RenderCommand.h"
#include "Crowny/Renderer/Renderer2D.h"
#include "Crowny/Renderer/Renderer.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <freetype-gl.h>

namespace Crowny
{

	constexpr glm::vec2 QuadUv[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
	constexpr glm::vec4 QuadVertices[] = { { -0.5f, -0.5f, 0.0f, 1.0f }, { 0.5f, -0.5f, 0.0f, 1.0f }, { 0.5f,  0.5f, 0.0f, 1.0f }, { -0.5f,  0.5f, 0.0f, 1.0f } };

	constexpr const char* RequiredSystemUniforms[2] = { "cw_ProjectionMatrix", "cw_ViewMatrix" };

	enum SystemUniformIndices : int32_t
	{
		UniformIndex_ProjectionMatrix = 0,
		UniformIndex_ViewMatrix = 1
	};

	struct UniformBuffer
	{
		byte* Buffer;
		uint32_t Size;

		UniformBuffer() { }
		UniformBuffer(byte* buffer, uint32_t size) : Buffer(buffer), Size(size) { memset(Buffer, 0, Size); }

	};

	struct Renderer2DSystemUniform
	{
		UniformBuffer Buffer;
		uint32_t Offset;

		Renderer2DSystemUniform() { }
		Renderer2DSystemUniform(const UniformBuffer& buffer, uint32_t offset) : Buffer(buffer), Offset(offset) { }
	};

	struct Renderer2DData
	{ // TODO: RenderCaps
		Ref<VertexArray> VertexArray;
		Ref<VertexBuffer> VertexBuffer;
		Ref<Shader> Shader;
		Ref<Texture2D> WhiteTexture;

		uint32_t IndexCount = 0;
		VertexData* Buffer = nullptr;

		std::array<Ref<Texture2D>, 32> TextureSlots;
		uint32_t TextureIndex = 0;
		
		std::vector<Renderer2DSystemUniform> SystemUniforms;
		std::vector<UniformBuffer> SystemUniformBuffers;
	};

	static Renderer2DData* s_Data = nullptr;

	void Renderer2D::Init()
	{
		s_Data = new Renderer2DData();
		s_Data->VertexArray = VertexArray::Create();
		s_Data->VertexBuffer = VertexBuffer::Create(nullptr, RENDERER_BUFFER_SIZE, { BufferUsage::DYNAMIC_DRAW });
		BufferLayout layout = { { ShaderDataType::Float4, "a_Coordinates" },
								{ ShaderDataType::Float2, "a_Uvs" },
								{ ShaderDataType::Float, "a_Tid" },
								{ ShaderDataType::Float4, "a_Color" } };

		s_Data->VertexBuffer->SetLayout(layout);
		s_Data->VertexArray->AddVertexBuffer(s_Data->VertexBuffer);

		uint32_t* indices = new uint32_t[RENDERER_INDICES_SIZE];

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

		Ref<IndexBuffer> ibo = IndexBuffer::Create(indices, RENDERER_INDICES_SIZE);
		s_Data->VertexArray->SetIndexBuffer(ibo);

		s_Data->WhiteTexture = Texture2D::Create(1, 1);
		uint32_t whiteTextureData = 0xffffffff;
		s_Data->WhiteTexture->SetData(&whiteTextureData, sizeof(uint32_t));

		s_Data->Shader = Shader::Create("/Shaders/BatchRenderer.glsl");
		s_Data->Shader->Bind();
		s_Data->SystemUniforms.resize(2);

		const ShaderUniformBufferList& vsuniforms = s_Data->Shader->GetVSSystemUniforms();

		for (auto* buffdecl : vsuniforms)
		{
			UniformBuffer buffer(new byte[buffdecl->GetSize()], buffdecl->GetSize());
			s_Data->SystemUniformBuffers.push_back(buffer);

			for (auto* decl : buffdecl->GetUniformDeclarations())
			{
				for (uint32_t j = 0; j < 2; j++)
				{
					if (decl->GetName() == RequiredSystemUniforms[j])
						s_Data->SystemUniforms[j] = Renderer2DSystemUniform(buffer, decl->GetOffset());
				}
			}
		}
		
		s_Data->TextureSlots[s_Data->TextureIndex] = s_Data->WhiteTexture;

		s_Data->VertexArray->Unbind();
		delete[] indices;
	}

	void Renderer2D::Begin(const Camera& camera, const glm::mat4& viewMatrix)
	{
		RenderCommand::SetDepthTest(false);
		memcpy(s_Data->SystemUniforms[UniformIndex_ProjectionMatrix].Buffer.Buffer + s_Data->SystemUniforms[UniformIndex_ProjectionMatrix].Offset, glm::value_ptr(camera.GetProjection()), sizeof(glm::mat4));
		memcpy(s_Data->SystemUniforms[UniformIndex_ViewMatrix].Buffer.Buffer + s_Data->SystemUniforms[UniformIndex_ViewMatrix].Offset, glm::value_ptr(viewMatrix), sizeof(glm::mat4));

		s_Data->Shader->Bind();
		//s_Data->Shader->SetVSSystemUniformBuffer(s_Data->SystemUniforms);
		s_Data->Shader->SetUniformMat4("cw_ProjectionMatrix", camera.GetProjection());
		s_Data->Shader->SetUniformMat4("cw_ViewMatrix", viewMatrix);
		s_Data->Buffer = (VertexData*)s_Data->VertexBuffer->GetPointer(RENDERER_MAX_SPRITES * 4);
	}

	float Renderer2D::FindTexture(const Ref<Texture2D>& texture)
	{
		if (!texture)
			return 0;

		float ts = 0.0f;

		for (uint8_t i = 1; i <= s_Data->TextureIndex; i++)
		{
			if (*s_Data->TextureSlots[i] == *texture)
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
				s_Data->Buffer = (VertexData*)s_Data->VertexBuffer->GetPointer(RENDERER_MAX_SPRITES * 4); // TODO: Begin or semething instead of this
			}
			s_Data->TextureSlots[++s_Data->TextureIndex] = texture;
			ts = (float)s_Data->TextureIndex;
		}
		return ts;
	}
	
	void Renderer2D::FillRect(const Rectangle& bounds, const glm::vec4& color)
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), 
											{ bounds.X, bounds.Y, 1.0f }) *
											glm::scale(glm::mat4(1.0f), 
											{ bounds.Width, bounds.Height, 1.0f });

		FillRect(transform, nullptr, color);
	}

	void Renderer2D::FillRect(const glm::mat4& transform, const Ref<Texture2D>& texture, const glm::vec4& color)
	{
		float ts = FindTexture(texture);
		for (uint8_t i = 0; i < 4; i++) {
			s_Data->Buffer->Position = QuadVertices[i] * transform;
			s_Data->Buffer->Uv = QuadUv[i];
			s_Data->Buffer->Tid = ts;
			s_Data->Buffer->Color = color;
			s_Data->Buffer++;
		}

		s_Data->IndexCount += 6;
	}

	void Renderer2D::FillRect(const Rectangle& bounds, const Ref<Texture2D>& texture, const glm::vec4& color)
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), { bounds.X, bounds.Y, 1.0f }) * glm::scale(glm::mat4(1.0f), { bounds.Width, bounds.Height, 1.0f });

		FillRect(transform, texture, color);
	}

	void Renderer2D::DrawString(const std::string& text, float x, float y, const Ref<Font>& font, const glm::vec4& color)
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
				float y0 = y - glyph->offset_y;
				float x1 = x0 + glyph->width;
				float y1 = y0 + glyph->height;

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

				s_Data->IndexCount += 6;

				x += glyph->advance_x;
			}
		}
	}

	void Renderer2D::DrawString(const std::string& text, const glm::mat4& transform, const Ref<Font>& font, const glm::vec4& color)
	{
		float x = transform[3][0];
		float y = transform[3][1];

		DrawString(text, x, y, font, color);
	}

	void Renderer2D::End()
	{
		s_Data->VertexBuffer->FreePointer();
		Flush();
		s_Data->IndexCount = 0;
		s_Data->TextureIndex = 0;
		RenderCommand::SetDepthTest(true);
	}

	void Renderer2D::Flush()
	{
		s_Data->Shader->Bind();
		for (uint32_t i = 0; i < s_Data->SystemUniformBuffers.size(); i++)
		{
			s_Data->Shader->SetVSSystemUniformBuffer(s_Data->SystemUniformBuffers[i].Buffer, s_Data->SystemUniformBuffers[i].Size, i);
		}

		for (uint8_t i = 0; i <= s_Data->TextureIndex; i++)
		{
			s_Data->TextureSlots[i]->Bind(i);
		}

		s_Data->VertexArray->Bind();
		RenderCommand::DrawIndexed(s_Data->VertexArray, s_Data->IndexCount);
		
		for (uint8_t i = 0; i <= s_Data->TextureIndex; i++)
		{
			s_Data->TextureSlots[i]->Unbind(i);
		}
		s_Data->VertexArray->Unbind();
	}

	void Renderer2D::Shutdown()
	{
		delete s_Data;
		s_Data = nullptr;
	}
}
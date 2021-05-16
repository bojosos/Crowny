#include "cwpch.h"

#include "Crowny/Renderer/IDBufferRenderer.h"
#include "Crowny/Renderer/Framebuffer.h"
#include "Crowny/Renderer/RenderCommand.h"

#include <glad/glad.h>
#include <entt/entt.hpp>

namespace Crowny
{
    constexpr glm::vec2 QuadUv[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
	constexpr glm::vec4 QuadVertices[] = { { -0.5f, -0.5f, 0.0f, 1.0f }, { 0.5f, -0.5f, 0.0f, 1.0f }, 
										  { 0.5f,  0.5f, 0.0f, 1.0f }, { -0.5f,  0.5f, 0.0f, 1.0f } };

	struct IDBufferRendererData
	{
		Ref<Shader> Shader2D;
        Ref<Shader> Shader3D;
        Ref<Framebuffer> Framebuffer;
            // For 2D batch rendering
		Ref<VertexArray> VertexArray;
		Ref<VertexBuffer> VertexBuffer;
    
		uint32_t IndexCount = 0;
		IDBufferRenderer::IDBufferData* Buffer = nullptr;
	};

	static IDBufferRendererData s_Data;
    
    void IDBufferRenderer::Init()
    {
        s_Data.Shader2D = Shader::Create("/Shaders/IDBufferShader.glsl");
        s_Data.Shader3D = Shader::Create("/Shaders/IDBuffer3DShader.glsl");
        
		FramebufferProperties fbProps;
		fbProps.Width = 681;
		fbProps.Height = 355;
		fbProps.Attachments = { FramebufferTextureFormat::R32I };
        s_Data.Framebuffer = Framebuffer::Create(fbProps);

        s_Data.VertexArray = VertexArray::Create();
		s_Data.VertexBuffer = VertexBuffer::Create(sizeof(IDBufferData) * 4 * RENDERER_MAX_SPRITES, BufferUsage::DYNAMIC_DRAW);
		BufferLayout layout = { { ShaderDataType::Float4, "a_Coordinates" },
								{ ShaderDataType::Int, "a_ObjectID" } };

		s_Data.VertexBuffer->SetLayout(layout);
		s_Data.VertexArray->AddVertexBuffer(s_Data.VertexBuffer);

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
		s_Data.VertexArray->SetIndexBuffer(ibo);
        delete[] indices;
    }
	
	void IDBufferRenderer::OnResize(uint32_t width, uint32_t height)
	{
		s_Data.Framebuffer->Resize(width, height);
	}

    void IDBufferRenderer::Begin(const glm::mat4& projection, const glm::mat4& view)
    {
        s_Data.Framebuffer->Bind();
		s_Data.Framebuffer->GetColorAttachment(0)->Clear((int32_t)entt::entity(entt::null));
  		s_Data.Buffer = (IDBufferData*)s_Data.VertexBuffer->Map(0, RENDERER_MAX_SPRITES * 4, GpuLockOptions::WRITE_DISCARD);
		s_Data.Shader3D->Bind();
		s_Data.Shader3D->SetUniformMat4("u_ProjectionMatrix", projection);
		s_Data.Shader3D->SetUniformMat4("u_ViewMatrix", view);
    }
    
    void IDBufferRenderer::DrawQuad(const glm::mat4& transform, uint32_t entityId)
    {
		for (uint8_t i = 0; i < 4; i++) {
			s_Data.Buffer->Position = QuadVertices[i] * transform;
			s_Data.Buffer->ObjectID = entityId;
			s_Data.Buffer++;
		}
		s_Data.IndexCount += 6;
    }

    void IDBufferRenderer::DrawMesh(const Ref<Mesh>& mesh, const glm::mat4& transform, uint32_t entityId)
    {
        s_Data.Shader3D->Bind();
        s_Data.Shader3D->SetUniformMat4("u_ModelMatrix", transform);
		s_Data.Shader3D->SetUniformInt("ObjectID", (int32_t)entityId);
		mesh->GetVertexArray()->Bind();
        //CW_ENGINE_INFO("Here: {0}", mesh->GetVertexArray()->GetIndexBuffer()->GetCount());
        RenderCommand::DrawIndexed(mesh->GetVertexArray(), mesh->GetVertexArray()->GetIndexBuffer()->GetCount());
    }
    
    void IDBufferRenderer::End()
    {
        s_Data.Shader2D->Bind();
		s_Data.VertexArray->Bind();
        s_Data.VertexBuffer->Unmap();
		RenderCommand::DrawIndexed(s_Data.VertexArray, s_Data.IndexCount);
		s_Data.IndexCount = 0;
		s_Data.VertexArray->Unbind();
        s_Data.Framebuffer->Unbind();
	}
    
    int32_t IDBufferRenderer::ReadPixel(int32_t x, int32_t y)
    {
		s_Data.Framebuffer->Bind();
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        int32_t pixelValue;
		//CW_ENGINE_INFO("{0}, {1}", x, y);
        glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &pixelValue);
		s_Data.Framebuffer->Unbind();
        return pixelValue;
    }
}

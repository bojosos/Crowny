#include "cwpch.h"

#include "Crowny/Renderer/ForwardRenderer.h"
#include "Crowny/Renderer/RenderCommand.h"
#include "../../Crowny-Editor/Source/Panels/ImGuiMaterialPanel.h"

namespace Crowny
{
	struct ForwardRendererData
	{
		byte* VSSystemUniformBuffer = nullptr;
		uint32_t VSSystemUniformBufferSize = 0;
		byte* FSSystemUniformBuffer = nullptr;
		uint32_t FSSystemUniformBufferSize = 0;

		std::vector<uint32_t> VSSystemUniformBufferOffsets;
		std::vector<uint32_t> FSSystemUniformBufferOffsets;
	};

	enum VSSystemUniformIndices : int32_t
	{
		VSSystemUniformIndex_ProjectionMatrix = 0,
		VSSystemUniformIndex_ViewMatrix = 1,
		VSSystemUniformIndex_ModelMatrix = 2,
		VSSystemUniformIndex_CameraPosition = 3,
		VSSystemUniformIndex_Size
	};

	enum FSSystemUniformIndices : int32_t
	{
		FSSystemUniformIndex_Lights = 0,
		FSSystemUniformIndex_Size
	};

	static ForwardRendererData s_Data;

	void ForwardRenderer::Init()
	{
		s_Data.VSSystemUniformBufferSize = sizeof(glm::mat4) + sizeof(glm::mat4) + sizeof(glm::vec3) + sizeof(glm::mat4);
		s_Data.VSSystemUniformBuffer = new byte[s_Data.VSSystemUniformBufferSize];
		memset(s_Data.VSSystemUniformBuffer, 0 , s_Data.VSSystemUniformBufferSize);
		s_Data.VSSystemUniformBufferOffsets.resize(VSSystemUniformIndex_Size);

		s_Data.VSSystemUniformBuffer[VSSystemUniformIndex_ProjectionMatrix] = 0;
		s_Data.VSSystemUniformBufferOffsets[VSSystemUniformIndex_ViewMatrix] = s_Data.VSSystemUniformBufferOffsets[VSSystemUniformIndex_ProjectionMatrix] + sizeof(glm::mat4);
		s_Data.VSSystemUniformBufferOffsets[VSSystemUniformIndex_ModelMatrix] = s_Data.VSSystemUniformBufferOffsets[VSSystemUniformIndex_ViewMatrix] + sizeof(glm::mat4);

		s_Data.VSSystemUniformBufferOffsets[VSSystemUniformIndex_CameraPosition] = s_Data.VSSystemUniformBufferOffsets[VSSystemUniformIndex_ModelMatrix] + sizeof(glm::mat4);
		/*
		s_Data.FSSystemUniformBufferSize = sizeof(Light);
		s_Data.FSSystemUniformBuffer = new byte[s_Data.FSSystemUniformBufferSize];
		memset(s_Data.FSSystemUniformBuffer, 0, s_Data.FSSystemUniformBufferSize);
		s_Data.FSSystemUniformBufferOffsets.resize(FSSystemUniformIndex_Size);

		// Per Scene System Uniforms
		s_Data.FSSystemUniformBufferOffsets[FSSystemUniformIndex_Lights] = 0;*/
	}

	void ForwardRenderer::Begin()
	{
		
	}

	void ForwardRenderer::BeginScene(Camera* camera, const glm::mat4& transform)
	{
		memcpy(s_Data.VSSystemUniformBuffer + s_Data.VSSystemUniformBufferOffsets[VSSystemUniformIndex_ProjectionMatrix], &camera->GetProjection(), sizeof(glm::mat4));
		memcpy(s_Data.VSSystemUniformBuffer + s_Data.VSSystemUniformBufferOffsets[VSSystemUniformIndex_ViewMatrix], &transform, sizeof(glm::mat4));
		glm::vec3 pos = glm::vec3(transform[3]);
		memcpy(s_Data.VSSystemUniformBuffer + s_Data.VSSystemUniformBufferOffsets[VSSystemUniformIndex_CameraPosition], &pos, sizeof(glm::vec3));
	}

	void ForwardRenderer::SetSystemUniforms(const Ref<Shader>& shader)
	{
		shader->SetVSSystemUniformBuffer(s_Data.VSSystemUniformBuffer, s_Data.VSSystemUniformBufferSize, 0);
		shader->SetFSSystemUniformBuffer(s_Data.FSSystemUniformBuffer, s_Data.FSSystemUniformBufferSize, 0);
	}

	void ForwardRenderer::SubmitLightSetup()
	{
		glm::vec3 lightPositions[] = {
			glm::vec3(-10.0f,  10.0f, 10.0f),
			glm::vec3(10.0f,  10.0f, 10.0f),
			glm::vec3(-10.0f, -10.0f, 10.0f),
			glm::vec3(10.0f, -10.0f, 10.0f),
		};
		glm::vec3 lightColors[] = {
			glm::vec3(300.0f, 300.0f, 300.0f),
			glm::vec3(300.0f, 300.0f, 300.0f),
			glm::vec3(300.0f, 300.0f, 300.0f),
			glm::vec3(300.0f, 300.0f, 300.0f)
		};

		ImGuiMaterialPanel::GetSlectedMaterial()->SetUniform("lightPositions", lightPositions);
		ImGuiMaterialPanel::GetSlectedMaterial()->SetUniform("lightColors", lightColors);
	}

	void ForwardRenderer::Submit(const Ref<Model>& model)
	{
		auto& texs = model->GetTextures();
		for (uint32_t i = 0; i < texs.size(); i++)
		{
			texs[i]->Bind(i);
		}

		for (auto& mesh : model->GetMeshes())
		{
			mesh->GetMaterialInstance()->Bind();
			mesh->GetVertexArray()->Bind();
			RenderCommand::DrawIndexed(mesh->GetVertexArray());
			mesh->GetMaterialInstance()->Unbind();
			mesh->GetVertexArray()->Unbind();
		}
	}

	void ForwardRenderer::SubmitMesh(const Ref<Mesh>& mesh, const glm::mat4& transform)
	{
		mesh->GetMaterialInstance()->Bind();
		memcpy(s_Data.VSSystemUniformBuffer + s_Data.VSSystemUniformBufferOffsets[VSSystemUniformIndex_ModelMatrix], &transform, sizeof(glm::mat4));
		SetSystemUniforms(mesh->GetMaterialInstance()->GetMaterial()->GetShader());
		mesh->GetVertexArray()->Bind();
		RenderCommand::DrawIndexed(mesh->GetVertexArray(), DrawMode::TRIANGLE_STRIP, 0);
	}

	void ForwardRenderer::EndScene()
	{

	}

	void ForwardRenderer::End()
	{

	}

	void ForwardRenderer::Flush()
	{
		
	}

}
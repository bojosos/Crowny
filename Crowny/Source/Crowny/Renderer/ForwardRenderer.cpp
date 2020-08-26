#include "cwpch.h"

#include "Crowny/Renderer/ForwardRenderer.h"

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
	}

	void ForwardRenderer::Begin()
	{

	}

	void ForwardRenderer::BeginScene(Camera* camera, const glm::mat4& transform)
	{
		memcpy(s_Data.VSSystemUniformBuffer + s_Data.VSSystemUniformBufferOffsets[VSSystemUniformIndex_ProjectionMatrix], &camera->GetProjectionMatrix(), sizeof(glm::mat4));
		memcpy(s_Data.VSSystemUniformBuffer + s_Data.VSSystemUniformBufferOffsets[VSSystemUniformIndex_ViewMatrix], &glm::inverse(transform), sizeof(glm::mat4));
		memcpy(s_Data.VSSystemUniformBuffer + s_Data.VSSystemUniformBufferOffsets[VSSystemUniformIndex_CameraPosition], &transform, sizeof(glm::vec3));
	}

	void ForwardRenderer::SetSystemUniforms(const Ref<Shader>& shader)
	{
		shader->SetVSSystemUniformBuffer(s_Data.VSSystemUniformBuffer, s_Data.VSSystemUniformBufferSize, 0);
		shader->SetFSSystemUniformBuffer(s_Data.FSSystemUniformBuffer, s_Data.FSSystemUniformBufferSize, 0);
	}

	void ForwardRenderer::SubmitMesh(const Ref<Mesh>& mesh, const glm::mat4& transform)
	{

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
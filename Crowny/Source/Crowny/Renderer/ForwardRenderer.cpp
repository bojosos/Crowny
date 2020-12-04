#include "cwpch.h"

#include "Crowny/Renderer/ForwardRenderer.h"
#include "Crowny/Renderer/RenderCommand.h"
#include "../../Crowny-Editor/Source/Panels/ImGuiMaterialPanel.h"

#include <glad/glad.h>

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

		Ref<Shader> BackgroundShader = nullptr;
		Ref<EnvironmentMap> Envmap = nullptr;
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
		
		s_Data.FSSystemUniformBufferSize = 8 * sizeof(glm::vec3);
		s_Data.FSSystemUniformBuffer = new byte[s_Data.FSSystemUniformBufferSize];
		memset(s_Data.FSSystemUniformBuffer, 0, s_Data.FSSystemUniformBufferSize);
		s_Data.FSSystemUniformBufferOffsets.resize(FSSystemUniformIndex_Size);

		// Per Scene System Uniforms
		s_Data.FSSystemUniformBufferOffsets[FSSystemUniformIndex_Lights] = 0;

		s_Data.BackgroundShader = Shader::Create("/Shaders/Background.glsl");
		s_Data.Envmap = EnvironmentMap::Create("/Textures/envmap.hdr");
	}

	void ForwardRenderer::Begin()
	{
		
	}

	void ForwardRenderer::BeginScene(Camera* camera, const glm::mat4& transform)
	{
		s_Data.Envmap->Bind(0);
		byte* data = new byte[sizeof(glm::mat4) * 2];
		memcpy(data, &camera->GetProjection(), sizeof(glm::mat4));
		auto inv = glm::inverse(transform);
		memcpy(data + sizeof(glm::mat4), &inv, sizeof(glm::mat4));
		s_Data.BackgroundShader->Bind();
		s_Data.BackgroundShader->SetVSSystemUniformBuffer(data, sizeof(glm::mat4) * 2, 0);
		delete[] data;
		memcpy(s_Data.VSSystemUniformBuffer + s_Data.VSSystemUniformBufferOffsets[VSSystemUniformIndex_ProjectionMatrix], &camera->GetProjection(), sizeof(glm::mat4));
		memcpy(s_Data.VSSystemUniformBuffer + s_Data.VSSystemUniformBufferOffsets[VSSystemUniformIndex_ViewMatrix], &inv, sizeof(glm::mat4)); // no inverse?
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
			mesh->GetMaterialInstance()->Bind(1);
			mesh->GetVertexArray()->Bind();
			RenderCommand::DrawIndexed(mesh->GetVertexArray());
			mesh->GetMaterialInstance()->Unbind();
			mesh->GetVertexArray()->Unbind();
		}
	}

	void ForwardRenderer::SubmitMesh(const Ref<Mesh>& mesh, const glm::mat4& transform)
	{
		mesh->GetMaterialInstance()->Bind(3);
		memcpy(s_Data.VSSystemUniformBuffer + s_Data.VSSystemUniformBufferOffsets[VSSystemUniformIndex_ModelMatrix], &transform, sizeof(glm::mat4));
		SetSystemUniforms(mesh->GetMaterialInstance()->GetMaterial()->GetShader());
		s_Data.Envmap->Bind(0);
		mesh->GetVertexArray()->Bind();
		SubmitLightSetup();
		RenderCommand::DrawIndexed(mesh->GetVertexArray(), DrawMode::TRIANGLE_STRIP, 0); // for spheres had to use triangle_strip
	}

	void ForwardRenderer::EndScene()
	{
		s_Data.BackgroundShader->Bind();
		s_Data.Envmap->BindSkybox(0);
		static unsigned int cubeVAO = 0;
		static unsigned int cubeVBO = 0;
    	if (cubeVAO == 0)
    	{
        	float vertices[] = {
				// back face
				-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
				1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
				1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
				1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
				-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
				-1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
				// front face
				-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
				1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
				1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
				1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
				-1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
				-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
				// left face
				-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
				-1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
				-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
				-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
				-1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
				-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
				// right face
				1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
				1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
				1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
				1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
				1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
				1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
				// bottom face
				-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
				1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
				1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
				1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
				-1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
				-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
				// top face
				-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
				1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
				1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
				1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
				-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
				-1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
			};
			glGenVertexArrays(1, &cubeVAO);
			glGenBuffers(1, &cubeVBO);
			// fill buffer
			glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
			// link vertex attributes
			glBindVertexArray(cubeVAO);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindVertexArray(0);
		}
		// render Cube
		glBindVertexArray(cubeVAO);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glBindVertexArray(0);
	}

	void ForwardRenderer::End()
	{

	}

	void ForwardRenderer::Flush()
	{
		
	}

}
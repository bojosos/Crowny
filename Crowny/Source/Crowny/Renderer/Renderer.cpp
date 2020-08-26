#include "cwpch.h"

#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/Renderer2D.h"
#include "Crowny/Renderer/RenderCommand.h"
#include "Crowny/Renderer/Camera.h"

#ifdef MC_WEB
#include <GLES3/gl32.h>
#else
#include <glad/glad.h>
#endif

namespace Crowny
{

	void Renderer::Init()
	{
		RenderCommand::Init();
		Renderer2D::Init();
	}

	void Renderer::Shutdown()
	{
		//BatchRenderer2D::Shutdown();
	}

	void Renderer::OnWindowResize(uint32_t width, uint32_t height)
	{
		RenderCommand::SetViewport(0, 0, width, height);
	}

	void Renderer::BeginScene()
	{
		
	}

	void Renderer::EndScene()
	{

	}

	void Renderer::Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform)
	{
		shader->Bind();

		//shader->SetMat4("u_ViewMatrix", Camera::GetCurrentCamera().GetViewMatrix());
		//shader->SetMat4("u_ProjectionMatrix", Camera::GetCurrentCamera().GetProjectionMatrix());

		//shader->SetMat4("u_Transform", transform); // use model matrix?

		vertexArray->Bind();
		RenderCommand::DrawIndexed(vertexArray);
	}

}
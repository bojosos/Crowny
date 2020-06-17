#include "cwpch.h"

#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/BatchRenderer2D.h"
#include "Crowny/Renderer/RenderCommand.h"
<<<<<<< HEAD
#include "Crowny/Renderer/Camera.h"
=======
>>>>>>> 8d51831a55da8001ceaabdbd722f54bfd1f9b2a2

#ifdef MC_WEB
//#include <GLFW/glfw3.h>
#include <GLES3/gl32.h>
#else
#include <glad/glad.h>
#endif

namespace Crowny
{

	void Renderer::Init()
	{
		RenderCommand::Init();
		BatchRenderer2D::Init();
	}

	void Renderer::Shutdown()
	{
		//BatchRenderer2D::Shutdown();
	}

	void Renderer::OnWindowResize(uint32_t width, uint32_t height)
	{
		RenderCommand::SetViewport(0, 0, width, height);
	}

<<<<<<< HEAD
	void Renderer::BeginScene()
	{
		
=======
	void Renderer::BeginScene(const Camera& camera)
	{
		s_SceneData->ViewProjectionMatrix = camera.GetViewProjectionMatrix();
>>>>>>> 8d51831a55da8001ceaabdbd722f54bfd1f9b2a2
	}

	void Renderer::EndScene()
	{

	}

	void Renderer::Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform)
	{
		shader->Bind();
<<<<<<< HEAD

		shader->SetMat4("u_ViewMatrix", Camera::GetCurrentCamera().GetViewMatrix());
		shader->SetMat4("u_ProjectionMatrix", Camera::GetCurrentCamera().GetProjectionMatrix());

=======
		shader->SetMat4("u_ViewProjection", s_SceneData->ViewProjectionMatrix);
>>>>>>> 8d51831a55da8001ceaabdbd722f54bfd1f9b2a2
		shader->SetMat4("u_Transform", transform);

		vertexArray->Bind();
		RenderCommand::DrawIndexed(vertexArray);
	}

}
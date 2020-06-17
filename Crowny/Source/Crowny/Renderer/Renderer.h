#pragma once

#include "Crowny/Renderer/RendererAPI.h"
#include "Crowny/Renderer/Shader.h"
<<<<<<< HEAD
=======
#include "Crowny/Renderer/Camera.h"
>>>>>>> 8d51831a55da8001ceaabdbd722f54bfd1f9b2a2

#include <glm/glm.hpp>

namespace Crowny
{

	class Renderer
	{
	public:
		static void Init();
		static void Shutdown();

		static void OnWindowResize(uint32_t width, uint32_t height);

<<<<<<< HEAD
		static void BeginScene();
=======
		static void BeginScene(const Camera& camera);
>>>>>>> 8d51831a55da8001ceaabdbd722f54bfd1f9b2a2

		static void Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform);

		static void EndScene();

		static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }

	};
}
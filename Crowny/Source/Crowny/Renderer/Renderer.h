#pragma once

#include "Crowny/Renderer/RendererAPI.h"
#include "Crowny/Renderer/Shader.h"

#include <glm/glm.hpp>

namespace Crowny
{

	class Renderer
	{
	public:
		static void Init();
		static void Shutdown();

		static void OnWindowResize(uint32_t width, uint32_t height);

		static void BeginScene();

		static void Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform);

		static void EndScene();

		static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }

	};
}
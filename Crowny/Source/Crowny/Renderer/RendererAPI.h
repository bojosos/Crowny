#pragma once

#include "Crowny/Renderer/VertexArray.h"

#include <glm/glm.hpp>

namespace Crowny {

	class RendererAPI
	{
	public:
		enum class API
		{
			None = 0, OpenGL = 1
		};

		virtual ~RendererAPI() = default;

	public:
		virtual void Init() = 0;
		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
		virtual void SetClearColor(const glm::vec4& color) = 0; // TODO: Replace with color
		virtual void Clear() = 0;

		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, DrawMode drawMode, uint32_t indexCount) = 0;

		static API GetAPI() { return s_API; }
		static Scope<RendererAPI> Create();
	private:
		static API s_API;
	};

}
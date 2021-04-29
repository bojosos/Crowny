#pragma once

#include "Crowny/Renderer/RendererAPI.h"

namespace Crowny
{

	class RenderCommand
	{
	public:
		static void Init()
		{
			s_RendererAPI->Init();
		}

		static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
		{
			//s_RendererAPI->SetViewport(x, y, width, height);
		}

		static void SetClearColor(const glm::vec4& color)
		{
			//s_RendererAPI->SetClearColor(color);
		}

		static void Clear()
		{
			//s_RendererAPI->Clear();
		}

		static void SetDepthTest(bool value)
		{
			//s_RendererAPI->SetDepthTest(value);
		}	

		static void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t count = -1)
		{
			//s_RendererAPI->DrawIndexed(vertexArray, count);
		}

		static void SwapBuffers()
		{
			//s_RendererAPI->SwapBuffers();
		}
		
	private:
		static Scope<RendererAPI> s_RendererAPI;

	};
}
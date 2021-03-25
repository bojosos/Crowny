#pragma once

#include "Crowny/Renderer/RendererAPI.h"

namespace Crowny
{
	class OpenGLRendererAPI : public RendererAPI
	{
	public:
		virtual void Init() override;
		virtual void Shutdown() override;
		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

		virtual void SetClearColor(const glm::vec4& color) override; // TODO: Replace with color
		virtual void SetDepthTest(bool value) override;
		virtual void Clear() override;

		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount) override;
		
	private:
		uint32_t m_NumDevices;
	};
}
#pragma once

#include "Crowny/Renderer/RendererAPI.h"

namespace Crowny
{
	class OpenGLRendererAPI : public RendererAPI
	{
	public:/*
		virtual void Init() override;
		virtual void Shutdown() override;
		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

		virtual void SetClearColor(const glm::vec4& color) override; // TODO: Replace with color
		virtual void SetDepthTest(bool value) override;
		virtual void Clear() override;

		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount) override;
		*/
        virtual void Init() override;
        virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override {};

        virtual void SetClearColor(const glm::vec4& color) override {};
        virtual void SetDepthTest(bool value) override {};
        virtual void SwapBuffers() override {};
        virtual void Clear() override {};

        virtual void SetGraphicsPipeline(const Ref<GraphicsPipeline>& pipeline) override {};
        virtual void SetComputePipeline(const Ref<ComputePipeline>& pipeline) override {} ; // TODO: Make virtua {}l
        virtual void SubmitCommandBuffer(const Ref<CommandBuffer>& commandBuffer) override {}; // TODO: Make virtual, create cmd buffer clas {}s
        virtual void DispatchCompute(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1) override {};
        virtual void SetIndexBuffer(const Ref<IndexBuffer>& buffer) override {};
        virtual void SetVertexBuffers(uint32_t idx, Ref<VertexBuffer>* buffers, uint32_t bufferCount) override {};
        virtual void Draw(uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount = 0) override {};
        virtual void DrawIndexed(uint32_t startIndex, uint32_t indexCount, uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount = 0) override {};
        virtual void SetRenderTarget(const Ref<Framebuffer>& target) override {};
        virtual void SetDrawMode(DrawMode drawMode) override {};

        virtual void Shutdown() override {};
	private:
		uint32_t m_NumDevices;
	};
}

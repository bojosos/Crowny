#pragma once

#include "Crowny/Common/Module.h"

#include "Crowny/Renderer/VertexArray.h"
#include "Crowny/Renderer/GraphicsPipeline.h"
#include "Crowny/Renderer/Framebuffer.h"
#include "Crowny/Renderer/VertexBuffer.h"
#include "Crowny/Renderer/GraphicsPipeline.h"
#include "Crowny/Renderer/CommandBuffer.h"

#include <glm/glm.hpp>

namespace Crowny {

	class RendererAPI : public Module<RendererAPI>
	{
	public:
		enum class API
		{
			None = 0,
			OpenGL = 1,
			Vulkan = 2
		};

		virtual ~RendererAPI() = default;

	public:
		virtual void Init() = 0;
		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
		virtual void SetClearColor(const glm::vec4& color) = 0; // TODO: Replace with color
		virtual void SetDepthTest(bool value) = 0;
		virtual void Clear() = 0;
		virtual void SwapBuffers() = 0;
		
		virtual void SubmitCommandBuffer(const Ref<CommandBuffer>& commandBuffer) = 0;
		virtual void SetGraphicsPipeline(const Ref<GraphicsPipeline>& pipeline) = 0;
		virtual void SetComputePipeline(const Ref<ComputePipeline>& pipeline) = 0;
		virtual void DispatchCompute(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1) = 0;
        virtual void SetIndexBuffer(const Ref<IndexBuffer>& buffer) = 0;
        virtual void SetVertexBuffers(uint32_t idx, Ref<VertexBuffer>* buffers, uint32_t bufferCount) = 0;
		virtual void Draw(uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount = 0) = 0;
		virtual void DrawIndexed(uint32_t startIndex, uint32_t indexCount, uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount = 0) = 0;
        virtual void SetRenderTarget(const Ref<Framebuffer>& target) = 0;
		virtual void SetDrawMode(DrawMode drawMode) = 0;

		virtual void Shutdown() = 0;
		
		static API GetAPI() { return s_API; }
		static Scope<RendererAPI> Create();
		
	protected:
		uint32_t VertexCountToPrimitiveCount(DrawMode drawMode, uint32_t elementCount);
		
	private:
		static API s_API;
	};

}
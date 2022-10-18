#pragma once

#include "Crowny/Common/Module.h"

#include <glm/glm.hpp>

namespace Crowny
{

    class RenderAPI : public Module<RenderAPI>
    {
    public:
        enum class API
        {
            None = 0,
            OpenGL = 1,
            Vulkan = 2
        };

        virtual ~RenderAPI() = default;

    public:
        virtual void Init() = 0;
        virtual void SwapBuffers(const Ref<RenderTarget>& renderTarget, uint32_t syncMask = 0xFFFFFFFF) = 0;

        virtual void SubmitCommandBuffer(const Ref<CommandBuffer>& commandBuffer, uint32_t syncMask = 0xFFFFFFFF) = 0;

        virtual void SetViewport(float x, float y, float width, float height,
                                 const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void SetClearColor(
          const glm::vec4& color) = 0; // TODO: Replace, const Ref<CommandBuffer>& commandBuffer = nullptr with color
        virtual void SetGraphicsPipeline(const Ref<GraphicsPipeline>& pipeline,
                                         const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void SetComputePipeline(const Ref<ComputePipeline>& pipeline,
                                        const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void DispatchCompute(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1,
                                     const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void SetIndexBuffer(const Ref<IndexBuffer>& buffer,
                                    const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void SetVertexBuffers(uint32_t idx, Ref<VertexBuffer>* buffers, uint32_t bufferCount,
                                      const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void ClearViewport(uint32_t buffers, const glm::vec4& color = glm::vec4(0.0f), float depth = 1.0f,
                                   uint16_t stencil = 0, uint8_t targetMask = 0xFF,
                                   const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void ClearRenderTarget(uint32_t buffers, const glm::vec4& color = glm::vec4(0.0f), float depth = 1.0f,
                                       uint16_t stencil = 0, uint8_t targetMask = 0xFF,
                                       const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void Draw(uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount = 0,
                          const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void DrawIndexed(uint32_t startIndex, uint32_t indexCount, uint32_t vertexOffset, uint32_t vertexCount,
                                 uint32_t instanceCount = 0, const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void SetRenderTarget(const Ref<RenderTarget>& target, uint32_t readOnlyFlags = 0,
                                     RenderSurfaceMask loadMask = RT_NONE,
                                     const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void SetDrawMode(DrawMode drawMode, const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void SetUniforms(const Ref<UniformParams>& params,
                                 const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;

        virtual void Shutdown() = 0;

        static API GetAPI() { return s_API; }
        static Scope<RenderAPI> Create();

    protected:
        uint32_t VertexCountToPrimitiveCount(DrawMode drawMode, uint32_t elementCount);

    private:
        static API s_API;
    };

} // namespace Crowny
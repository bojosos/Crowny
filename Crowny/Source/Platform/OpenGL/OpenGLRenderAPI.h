#pragma once

#include "Crowny/RenderAPI/RenderAPI.h"

namespace Crowny
{
    class OpenGLRenderAPI : public RenderAPI
    {
    public:
        virtual void Init() override;
        virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                                 const Ref<CommandBuffer>& commandBuffer = nullptr) override{};

        virtual void SetClearColor(const glm::vec4& color) override{};
        virtual void SwapBuffers(const Ref<RenderTarget>& renderTarget, uint32_t syncMask = 0xFFFFFFFF) override{};

        virtual void SetGraphicsPipeline(const Ref<GraphicsPipeline>& pipeline,
                                         const Ref<CommandBuffer>& commandBuffer = nullptr) override{};
        virtual void SetComputePipeline(const Ref<ComputePipeline>& pipeline,
                                        const Ref<CommandBuffer>& commandBuffer = nullptr) override{};
        virtual void SubmitCommandBuffer(const Ref<CommandBuffer>& commandBuffer,
                                         uint32_t syncMask = 0xFFFFFFFF) override{};
        virtual void DispatchCompute(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1,
                                     const Ref<CommandBuffer>& commandBuffer = nullptr) override{};
        virtual void SetIndexBuffer(const Ref<IndexBuffer>& buffer,
                                    const Ref<CommandBuffer>& commandBuffer = nullptr) override{};
        virtual void SetVertexBuffers(uint32_t idx, Ref<VertexBuffer>* buffers, uint32_t bufferCount,
                                      const Ref<CommandBuffer>& commandBuffer = nullptr) override{};
        virtual void ClearViewport(uint32_t buffers, const glm::vec4& color = glm::vec4(0.0f), float depth = 1.0f,
                                   uint16_t stencil = 0, uint8_t targetMask = 0xFF,
                                   const Ref<CommandBuffer>& commandBuffer = nullptr) override{};
        virtual void ClearRenderTarget(uint32_t buffers, const glm::vec4& color = glm::vec4(0.0f), float depth = 1.0f,
                                       uint16_t stencil = 0, uint8_t targetMask = 0xFF,
                                       const Ref<CommandBuffer>& commandBuffer = nullptr) override{};
        virtual void Draw(uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount = 0,
                          const Ref<CommandBuffer>& commandBuffer = nullptr) override{};
        virtual void DrawIndexed(uint32_t startIndex, uint32_t indexCount, uint32_t vertexOffset, uint32_t vertexCount,
                                 uint32_t instanceCount = 0,
                                 const Ref<CommandBuffer>& commandBuffer = nullptr) override{};
        virtual void SetRenderTarget(const Ref<RenderTarget>& target, uint32_t readOnlyFlags = 0,
                                     RenderSurfaceMask loadMask = RT_NONE,
                                     const Ref<CommandBuffer>& commandBuffer = nullptr) override{};
        virtual void SetDrawMode(DrawMode drawMode, const Ref<CommandBuffer>& commandBuffer = nullptr) override{};

        virtual void Shutdown() override{};

    private:
        uint32_t m_NumDevices;
    };
} // namespace Crowny

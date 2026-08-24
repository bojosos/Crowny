#pragma once

#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/RenderCapabilities.h"

namespace Crowny
{
    class OpenGLRenderAPI : public RenderAPI
    {
    public:
        OpenGLRenderAPI() : RenderAPI(API::OpenGL) {}

        void Init() override;
        const RenderCapabilities& GetCapabilities(uint32_t deviceIndex = 0) const override;
        void SetViewport(float x, float y, float width, float height, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        void SetScissorRect(const Rect2I& rect, const Ref<CommandBuffer>& commandBuffer) override;
        void SetClearColor(const glm::vec4& color) override;
        void SwapBuffers(const Ref<RenderTarget>& renderTarget, uint32_t syncMask = 0xFFFFFFFF) override;
        void SetGraphicsPipeline(const Ref<GraphicsPipeline>& pipeline, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        void SetRayTracingPipeline(const Ref<RayTracingPipeline>& pipeline, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        void SetComputePipeline(const Ref<ComputePipeline>& pipeline, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        void SubmitCommandBuffer(const Ref<CommandBuffer>& commandBuffer, uint32_t syncMask = 0xFFFFFFFF) override;
        void SetIndexBuffer(const Ref<IndexBuffer>& buffer, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        void SetVertexBuffers(uint32_t idx, Ref<VertexBuffer>* buffers, uint32_t bufferCount,
                              const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        void SetVertexLayout(const Ref<BufferLayout>& vertexLayout, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        void ClearViewport(uint32_t buffers, const glm::vec4& color = glm::vec4(0.0f), float depth = 1.0f, uint16_t stencil = 0,
                           uint8_t targetMask = 0xFF, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        void ClearRenderTarget(uint32_t buffers, const glm::vec4& color = glm::vec4(0.0f), float depth = 1.0f, uint16_t stencil = 0,
                               uint8_t targetMask = 0xFF, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        void Draw(uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount = 1,
                  const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        void DrawIndexed(uint32_t startIndex, uint32_t indexCount, uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount = 1,
                         const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        void DrawIndexedIndirect(const Ref<GenericGpuBuffer>& argumentBuffer, uint32_t argumentOffset, uint32_t drawCount,
                                 uint32_t stride = sizeof(DrawIndexedIndirectCommand),
                                 const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        void DrawIndexedIndirectCount(const Ref<GenericGpuBuffer>& argumentBuffer, uint32_t argumentOffset,
                                      const Ref<GenericGpuBuffer>& countBuffer, uint32_t countOffset, uint32_t maxDrawCount,
                                      uint32_t stride = sizeof(DrawIndexedIndirectCommand),
                                      const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        void TraceRays(uint32_t width, uint32_t height, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        void DispatchCompute(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1,
                             const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        void DispatchComputeIndirect(const Ref<GenericGpuBuffer>& argumentBuffer, uint32_t argumentOffset,
                                     const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        void SetRenderTarget(const Ref<RenderTarget>& target, uint32_t readOnlyFlags = 0, RenderSurfaceMask loadMask = RT_NONE,
                             const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        void SetDrawMode(DrawMode drawMode, const Ref<CommandBuffer>& commandBuffer = nullptr) override;
        void SetUniforms(const Ref<UniformParams>& params, const Ref<CommandBuffer>& commandBuffer = nullptr) override;

        void OnShutdown() override;

    private:
        void ConfigureVertexArray();
        void ApplyPipelineState();
        void ApplyViewport();
        static void RequireImmediate(const Ref<CommandBuffer>& commandBuffer);

        uint32_t m_VertexArray = 0;
        uint32_t m_EnabledAttributeCount = 0;
        DrawMode m_DrawMode = DrawMode::TRIANGLE_LIST;
        Rect2F m_Viewport = Rect2F(0.0f, 0.0f, 1.0f, 1.0f);
        glm::vec4 m_ClearColor = glm::vec4(0.0f);
        Vector<Ref<VertexBuffer>> m_VertexBuffers;
        Ref<IndexBuffer> m_IndexBuffer;
        Ref<BufferLayout> m_VertexLayout;
        Ref<GraphicsPipeline> m_GraphicsPipeline;
        Ref<ComputePipeline> m_ComputePipeline;
        Ref<RenderTarget> m_RenderTarget;
        uint32_t m_ReadOnlyFlags = 0;
        RenderCapabilities m_Capabilities;
    };
} // namespace Crowny

#pragma once

#include "Crowny/Common/Module.h"
#include "Crowny/RenderAPI/Buffer.h"

#include <glm/glm.hpp>

#include <atomic>
#include <mutex>

namespace Crowny
{
    class GenericGpuBuffer;
    class RenderCapabilities;

    struct DrawIndexedIndirectCommand
    {
        uint32_t IndexCount = 0;
        uint32_t InstanceCount = 0;
        uint32_t FirstIndex = 0;
        int32_t VertexOffset = 0;
        uint32_t FirstInstance = 0;
    };

    struct DispatchIndirectCommand
    {
        uint32_t GroupCountX = 0;
        uint32_t GroupCountY = 0;
        uint32_t GroupCountZ = 0;
    };

    struct RenderFrameStatistics
    {
        uint64_t FrameNumber = 0;
        float FrameTimeMs = 0.0f;
        float FramesPerSecond = 0.0f;
        uint64_t DrawCalls = 0;
        uint64_t DirectDrawCalls = 0;
        uint64_t IndirectDrawCalls = 0;
        uint64_t IndirectCommands = 0;
        uint64_t Vertices = 0;
        uint64_t Triangles = 0;
        uint64_t Instances = 0;
        uint64_t ComputeDispatches = 0;
        uint64_t RayTracingDispatches = 0;
    };

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
        virtual const RenderCapabilities& GetCapabilities(uint32_t deviceIndex = 0) const = 0;
        virtual void SwapBuffers(const Ref<RenderTarget>& renderTarget, uint32_t syncMask = 0xFFFFFFFF) = 0;

        virtual void SubmitCommandBuffer(const Ref<CommandBuffer>& commandBuffer, uint32_t syncMask = 0xFFFFFFFF) = 0;

        virtual void SetViewport(float x, float y, float width, float height, const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void SetScissorRect(const Rect2I& rect, const Ref<CommandBuffer>& commandBuffer) = 0;
        virtual void SetClearColor(const glm::vec4& color) = 0; // TODO: Replace, const Ref<CommandBuffer>& commandBuffer = nullptr with color
        virtual void SetGraphicsPipeline(const Ref<GraphicsPipeline>& pipeline, const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void SetRayTracingPipeline(const Ref<RayTracingPipeline>& pipeline, const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void SetComputePipeline(const Ref<ComputePipeline>& pipeline, const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void SetIndexBuffer(const Ref<IndexBuffer>& buffer, const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void SetVertexBuffers(uint32_t idx, Ref<VertexBuffer>* buffers, uint32_t bufferCount,
                                      const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void SetVertexLayout(const Ref<BufferLayout>& vertexLayout, const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void ClearViewport(uint32_t buffers, const glm::vec4& color = glm::vec4(0.0f), float depth = 1.0f, uint16_t stencil = 0,
                                   uint8_t targetMask = 0xFF, const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void ClearRenderTarget(uint32_t buffers, const glm::vec4& color = glm::vec4(0.0f), float depth = 1.0f, uint16_t stencil = 0,
                                       uint8_t targetMask = 0xFF, const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void Draw(uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount = 1,
                          const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void DrawIndexed(uint32_t startIndex, uint32_t indexCount, uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount = 1,
                                 const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void DrawIndexedIndirect(const Ref<GenericGpuBuffer>& argumentBuffer, uint32_t argumentOffset, uint32_t drawCount,
                                         uint32_t stride = sizeof(DrawIndexedIndirectCommand),
                                         const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void DrawIndexedIndirectCount(const Ref<GenericGpuBuffer>& argumentBuffer, uint32_t argumentOffset,
                                              const Ref<GenericGpuBuffer>& countBuffer, uint32_t countOffset, uint32_t maxDrawCount,
                                              uint32_t stride = sizeof(DrawIndexedIndirectCommand),
                                              const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void TraceRays(uint32_t width, uint32_t height, const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void DispatchCompute(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1,
                                     const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void DispatchComputeIndirect(const Ref<GenericGpuBuffer>& argumentBuffer, uint32_t argumentOffset,
                                             const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void SetRenderTarget(const Ref<RenderTarget>& target, uint32_t readOnlyFlags = 0, RenderSurfaceMask loadMask = RT_NONE,
                                     const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void SetDrawMode(DrawMode drawMode, const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;
        virtual void SetUniforms(const Ref<UniformParams>& params, const Ref<CommandBuffer>& commandBuffer = nullptr) = 0;

        // Publishes the counters accumulated since the previous call and starts a new frame.
        // The returned snapshot is safe to read while rendering continues on another thread.
        void BeginFrameStatistics(float frameTimeSeconds);
        RenderFrameStatistics GetFrameStatistics() const;

        static uint64_t GetPrimitiveCount(DrawMode drawMode, uint64_t elementCount);

    protected:
        explicit RenderAPI(API api) : m_API(api) {}

        void OnStartUp() override;
        void OnShutdown() override;

    public:
        static API GetAPI() { return s_API; }
        static Scope<RenderAPI> Create();

    protected:
        void RecordDraw(DrawMode drawMode, uint32_t elementCount, uint32_t instanceCount);
        void RecordIndirectDraw(uint32_t commandCount);
        void RecordComputeDispatch();
        void RecordRayTracingDispatch();

    private:
        struct StatisticsAccumulator
        {
            std::atomic<uint64_t> DrawCalls{ 0 };
            std::atomic<uint64_t> DirectDrawCalls{ 0 };
            std::atomic<uint64_t> IndirectDrawCalls{ 0 };
            std::atomic<uint64_t> IndirectCommands{ 0 };
            std::atomic<uint64_t> Vertices{ 0 };
            std::atomic<uint64_t> Triangles{ 0 };
            std::atomic<uint64_t> Instances{ 0 };
            std::atomic<uint64_t> ComputeDispatches{ 0 };
            std::atomic<uint64_t> RayTracingDispatches{ 0 };
        };

        API m_API;
        StatisticsAccumulator m_Statistics;
        mutable std::mutex m_CompletedStatisticsMutex;
        RenderFrameStatistics m_CompletedStatistics;
        float m_SmoothedFrameTimeMs = 0.0f;
        static API s_API;
    };
} // namespace Crowny

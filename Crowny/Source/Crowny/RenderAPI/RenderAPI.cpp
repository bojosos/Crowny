#include "cwpch.h"

#include "Crowny/RenderAPI/RenderAPI.h"
#include "Platform/OpenGL/OpenGLRenderAPI.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"

namespace Crowny
{
    RenderAPI::API RenderAPI::s_API = RenderAPI::API::None;

    void RenderAPI::OnStartUp()
    {
        s_API = m_API;
    }

    void RenderAPI::OnShutdown()
    {
        s_API = API::None;
    }

    uint64_t RenderAPI::GetPrimitiveCount(DrawMode drawMode, uint64_t elementCount)
    {
        switch (drawMode)
        {
        case DrawMode::POINT_LIST:
            return elementCount;
        case DrawMode::LINE_LIST:
            return elementCount / 2;
        case DrawMode::LINE_STRIP:
            return elementCount > 1 ? elementCount - 1 : 0;
        case DrawMode::TRIANGLE_LIST:
            return elementCount / 3;
        case DrawMode::TRIANGLE_STRIP:
            return elementCount > 2 ? elementCount - 2 : 0;
        case DrawMode::TRIANGLE_FAN:
            return elementCount > 2 ? elementCount - 2 : 0;
        }

        return 0;
    }

    void RenderAPI::BeginFrameStatistics(float frameTimeSeconds)
    {
        const float frameTimeMs = std::max(frameTimeSeconds * 1000.0f, 0.0f);
        if (frameTimeMs > 0.0f)
            m_SmoothedFrameTimeMs = m_SmoothedFrameTimeMs > 0.0f ? glm::mix(m_SmoothedFrameTimeMs, frameTimeMs, 0.1f) : frameTimeMs;

        RenderFrameStatistics completed;
        completed.FrameTimeMs = m_SmoothedFrameTimeMs;
        completed.FramesPerSecond = m_SmoothedFrameTimeMs > 0.0f ? 1000.0f / m_SmoothedFrameTimeMs : 0.0f;
        completed.DrawCalls = m_Statistics.DrawCalls.exchange(0, std::memory_order_relaxed);
        completed.DirectDrawCalls = m_Statistics.DirectDrawCalls.exchange(0, std::memory_order_relaxed);
        completed.IndirectDrawCalls = m_Statistics.IndirectDrawCalls.exchange(0, std::memory_order_relaxed);
        completed.IndirectCommands = m_Statistics.IndirectCommands.exchange(0, std::memory_order_relaxed);
        completed.Vertices = m_Statistics.Vertices.exchange(0, std::memory_order_relaxed);
        completed.Triangles = m_Statistics.Triangles.exchange(0, std::memory_order_relaxed);
        completed.Instances = m_Statistics.Instances.exchange(0, std::memory_order_relaxed);
        completed.ComputeDispatches = m_Statistics.ComputeDispatches.exchange(0, std::memory_order_relaxed);
        completed.RayTracingDispatches = m_Statistics.RayTracingDispatches.exchange(0, std::memory_order_relaxed);

        std::scoped_lock lock(m_CompletedStatisticsMutex);
        completed.FrameNumber = m_CompletedStatistics.FrameNumber + 1;
        m_CompletedStatistics = completed;
    }

    RenderFrameStatistics RenderAPI::GetFrameStatistics() const
    {
        std::scoped_lock lock(m_CompletedStatisticsMutex);
        return m_CompletedStatistics;
    }

    void RenderAPI::RecordDraw(DrawMode drawMode, uint32_t elementCount, uint32_t instanceCount)
    {
        if (elementCount == 0 || instanceCount == 0)
            return;

        const uint64_t submittedElements = static_cast<uint64_t>(elementCount) * instanceCount;
        m_Statistics.DrawCalls.fetch_add(1, std::memory_order_relaxed);
        m_Statistics.DirectDrawCalls.fetch_add(1, std::memory_order_relaxed);
        m_Statistics.Vertices.fetch_add(submittedElements, std::memory_order_relaxed);
        m_Statistics.Instances.fetch_add(instanceCount, std::memory_order_relaxed);
        if (drawMode == DrawMode::TRIANGLE_LIST || drawMode == DrawMode::TRIANGLE_STRIP || drawMode == DrawMode::TRIANGLE_FAN)
            m_Statistics.Triangles.fetch_add(GetPrimitiveCount(drawMode, elementCount) * instanceCount, std::memory_order_relaxed);
    }

    void RenderAPI::RecordIndirectDraw(uint32_t commandCount)
    {
        m_Statistics.DrawCalls.fetch_add(1, std::memory_order_relaxed);
        m_Statistics.IndirectDrawCalls.fetch_add(1, std::memory_order_relaxed);
        if (commandCount != 0)
            m_Statistics.IndirectCommands.fetch_add(commandCount, std::memory_order_relaxed);
    }

    void RenderAPI::RecordComputeDispatch() { m_Statistics.ComputeDispatches.fetch_add(1, std::memory_order_relaxed); }

    void RenderAPI::RecordRayTracingDispatch() { m_Statistics.RayTracingDispatches.fetch_add(1, std::memory_order_relaxed); }
} // namespace Crowny

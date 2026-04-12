#include "cwpch.h"

#include "Crowny/Renderer/RenderThread.h"
#include "Crowny/Scene/SceneRenderer.h"

#include <tracy/Tracy.hpp>

namespace Crowny
{

    RenderThread::RenderThread() = default;

    RenderThread::~RenderThread() { Stop(); }

    void RenderThread::Start()
    {
        if (m_Running.load(std::memory_order_acquire))
            return;

        m_Running.store(true, std::memory_order_release);
        m_Thread = Thread([this]() { RenderLoop(); });
    }

    void RenderThread::Stop()
    {
        if (!m_Running.load(std::memory_order_acquire))
            return;

        m_Running.store(false, std::memory_order_release);
        m_FrameSync.Shutdown();

        if (m_Thread.joinable())
            m_Thread.join();
    }

    void RenderThread::SubmitFrame(RenderSnapshot&& snapshot)
    {
        ZoneScopedN("SubmitFrame");
        // Wait for the render thread to finish the previous frame before we
        // overwrite the write buffer (ensures we never write while render reads).
        m_FrameSync.SimWaitForRenderDone();

        // Write snapshot to the write buffer
        uint32_t writeIdx = m_WriteIdx.load(std::memory_order_acquire);
        m_Snapshots[writeIdx] = std::move(snapshot);

        // Swap so the render thread reads from what we just wrote
        m_WriteIdx.store(writeIdx ^ 1, std::memory_order_release);

        // Also swap the resource command queue
        m_ResourceCmdQueue.Swap();

        // Signal render thread
        m_FrameSync.SimSignalNewFrame();
    }

    void RenderThread::WaitForFrameDone() { m_FrameSync.SimWaitForRenderDone(); }

    void RenderThread::EnqueueResourceCommand(std::function<void()>&& cmd) { m_ResourceCmdQueue.Enqueue(std::move(cmd)); }

    void RenderThread::RenderLoop()
    {
        tracy::SetThreadName("RenderThread");

        while (m_Running.load(std::memory_order_acquire))
        {
            // Wait for sim thread to provide a new snapshot
            if (!m_FrameSync.RenderWaitForNewFrame())
                break;

            FrameMarkStart("RenderThread");

            {
                ZoneScopedN("DrainResourceCmds");
                m_ResourceCmdQueue.DrainAndExecute();
            }

            {
                ZoneScopedN("RenderFromSnapshot");
                uint32_t readIdx = m_WriteIdx.load(std::memory_order_acquire) ^ 1;
                SceneRenderer::RenderFromSnapshot(m_Snapshots[readIdx]);
            }

            FrameMarkEnd("RenderThread");

            // Signal sim thread that we are done
            m_FrameSync.RenderSignalDone();
        }
    }

} // namespace Crowny

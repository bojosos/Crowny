#pragma once

#include "Crowny/Renderer/RenderSnapshot.h"
#include "Crowny/Threading/CommandQueue.h"
#include "Crowny/Threading/FrameSync.h"

namespace Crowny
{

    class SceneRenderer;

    class RenderThread
    {
    public:
        RenderThread();
        ~RenderThread();

        void Start();
        void Stop();

        // Called by sim thread: submits a snapshot for the render thread to consume.
        // Non-blocking — the snapshot is moved into the write buffer and the render thread is signaled.
        void SubmitFrame(RenderSnapshot&& snapshot);

        // Called by sim thread: blocks until the render thread finishes the current frame.
        void WaitForFrameDone();

        // Called by sim thread: enqueues a GPU resource command (create/destroy texture, etc.)
        // to be executed on the render thread.
        void EnqueueResourceCommand(std::function<void()>&& cmd);

        bool IsRunning() const { return m_Running.load(std::memory_order_acquire); }

    private:
        void RenderLoop();

        Thread m_Thread;
        FrameSync m_FrameSync;
        RenderSnapshot m_Snapshots[2];
        std::atomic<uint32_t> m_WriteIdx{ 0 };
        CommandQueue m_ResourceCmdQueue;
        std::atomic<bool> m_Running{ false };
    };

} // namespace Crowny

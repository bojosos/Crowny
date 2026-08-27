#pragma once

#include "Crowny/Renderer/FrameContext.h"
#include "Crowny/Renderer/RenderHistoryReleaseSink.h"

namespace Crowny
{

    class SceneRenderer;

    class RenderThread : public RenderHistoryReleaseSink
    {
    public:
        explicit RenderThread(uint32_t frameContextCount = 2);
        ~RenderThread() override;

        void Start();
        void Stop();

        // Returns the writable frame slot after the render thread has released it.
        // Extract directly into this snapshot, then call SubmitFrame().
        RenderSnapshot& BeginFrame();
        void SubmitFrame();

        // Called by sim thread: submits a snapshot for the render thread to consume.
        // Non-blocking — the snapshot is moved into the write buffer and the render thread is signaled.
        void SubmitFrame(RenderSnapshot&& snapshot);

        // Called by sim thread: blocks until the render thread finishes the current frame.
        void WaitForFrameDone();

        // Called by the simulation thread. The command owns its inputs until the render thread executes it.
        void EnqueueMeshUpload(MeshUploadCommand command);

        // Releases history after every frame submitted before this call has finished.
        void QueueHistoryRelease(uint64_t historyNamespace) override;

        bool IsRunning() const { return m_Running.load(std::memory_order_acquire); }
        uint32_t GetFrameContextCount() const { return static_cast<uint32_t>(m_FrameContexts.size()); }

    private:
        void RenderLoop();

        struct HistoryReleaseCommand
        {
            uint64_t HistoryNamespace = 0;
            uint64_t AfterSubmissionValue = 0;
        };

        enum class ContextState : uint8_t
        {
            Available,
            Recording,
            Ready,
            Rendering
        };

        Thread m_Thread;
        Vector<FrameContext> m_FrameContexts;
        Vector<ContextState> m_ContextStates;
        Deque<uint32_t> m_ReadyContexts;
        Deque<HistoryReleaseCommand> m_PendingHistoryReleases;
        Mutex m_ContextMutex;
        Signal m_ContextReady;
        Signal m_ContextAvailable;

        Vector<MeshUploadCommand> m_PendingMeshUploads;
        Mutex m_MeshUploadMutex;

        std::atomic<bool> m_Running{ false };
        uint32_t m_NextWriteContext = 0;
        uint32_t m_RecordingContext = 0;
        uint32_t m_RenderingContexts = 0;
        uint64_t m_NextSubmissionValue = 1;
        uint64_t m_LastCompletedSubmissionValue = 0;
        bool m_FrameOpen = false;
    };

} // namespace Crowny

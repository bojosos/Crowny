#include "cwpch.h"

#include "Crowny/Renderer/RenderThread.h"
#include "Crowny/Scene/SceneRenderer.h"

#include <tracy/Tracy.hpp>

namespace Crowny
{

    RenderThread::RenderThread(uint32_t frameContextCount)
    {
        frameContextCount = std::max(2u, frameContextCount);
        m_FrameContexts.reserve(frameContextCount);
        m_ContextStates.resize(frameContextCount, ContextState::Available);
        for (uint32_t index = 0; index < frameContextCount; index++)
            m_FrameContexts.emplace_back(index);
        m_PendingMeshUploads.reserve(64);
    }

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
        m_ContextReady.notify_all();
        m_ContextAvailable.notify_all();

        if (m_Thread.joinable())
            m_Thread.join();
    }

    RenderSnapshot& RenderThread::BeginFrame()
    {
        ZoneScopedN("BeginFrame");
        CW_ENGINE_ASSERT(!m_FrameOpen);

        Lock lock(m_ContextMutex);
        const uint32_t contextIndex = m_NextWriteContext;
        m_ContextAvailable.wait(lock, [&]() {
            return !m_Running.load(std::memory_order_acquire) || m_ContextStates[contextIndex] == ContextState::Available;
        });
        CW_ENGINE_ASSERT(m_Running.load(std::memory_order_acquire), "Cannot begin a frame on a stopped render thread");

        FrameContext& context = m_FrameContexts[contextIndex];
        context.BeginRecording(m_NextSubmissionValue);
        m_ContextStates[contextIndex] = ContextState::Recording;
        m_RecordingContext = contextIndex;
        m_FrameOpen = true;
        return context.Snapshot;
    }

    void RenderThread::SubmitFrame()
    {
        ZoneScopedN("SubmitFrame");
        CW_ENGINE_ASSERT(m_FrameOpen);

        {
            ScopedLock resourceLock(m_MeshUploadMutex);
            FrameContext& context = m_FrameContexts[m_RecordingContext];
            context.MeshUploadCommands.insert(context.MeshUploadCommands.end(), std::make_move_iterator(m_PendingMeshUploads.begin()),
                                              std::make_move_iterator(m_PendingMeshUploads.end()));
            m_PendingMeshUploads.clear();
        }

        {
            ScopedLock lock(m_ContextMutex);
            FrameContext& context = m_FrameContexts[m_RecordingContext];
            context.SubmissionValue = m_NextSubmissionValue++;
            m_ContextStates[m_RecordingContext] = ContextState::Ready;
            m_ReadyContexts.push_back(m_RecordingContext);
            m_NextWriteContext = (m_RecordingContext + 1u) % static_cast<uint32_t>(m_FrameContexts.size());
            m_FrameOpen = false;
        }
        m_ContextReady.notify_one();
    }

    void RenderThread::SubmitFrame(RenderSnapshot&& snapshot)
    {
        RenderSnapshot& target = BeginFrame();
        const uint64_t frameNumber = target.FrameNumber;
        target = std::move(snapshot);
        target.FrameNumber = frameNumber;
        SubmitFrame();
    }

    void RenderThread::WaitForFrameDone()
    {
        Lock lock(m_ContextMutex);
        m_ContextAvailable.wait(lock, [&]() {
            return !m_Running.load(std::memory_order_acquire) || (m_ReadyContexts.empty() && m_RenderingContexts == 0);
        });
    }

    void RenderThread::EnqueueMeshUpload(MeshUploadCommand command)
    {
        ScopedLock lock(m_MeshUploadMutex);
        m_PendingMeshUploads.push_back(std::move(command));
    }

    void RenderThread::QueueHistoryRelease(uint64_t historyNamespace)
    {
        if (historyNamespace == 0)
            return;

        {
            ScopedLock lock(m_ContextMutex);
            const uint64_t lastSubmittedValue = m_NextSubmissionValue - 1u;
            const uint64_t afterSubmissionValue = m_FrameOpen ? m_NextSubmissionValue : lastSubmittedValue;
            m_PendingHistoryReleases.push_back({ historyNamespace, afterSubmissionValue });
        }
        m_ContextReady.notify_one();
    }

    void RenderThread::RenderLoop()
    {
        tracy::SetThreadName("RenderThread");

        for (;;)
        {
            uint32_t contextIndex = 0;
            uint64_t historyNamespace = 0;
            {
                Lock lock(m_ContextMutex);
                m_ContextReady.wait(lock, [&]() {
                    const bool historyReleaseReady = !m_PendingHistoryReleases.empty() &&
                                                     m_PendingHistoryReleases.front().AfterSubmissionValue <=
                                                       m_LastCompletedSubmissionValue;
                    return !m_Running.load(std::memory_order_acquire) || !m_ReadyContexts.empty() || historyReleaseReady;
                });
                const bool stopping = !m_Running.load(std::memory_order_acquire);
                if (stopping && m_ReadyContexts.empty())
                {
                    if (m_PendingHistoryReleases.empty())
                        break;

                    // No submitted frame remains. Drain every release on this thread
                    // before its renderer resources are destroyed, even when a command
                    // was waiting for a recording frame that will never be submitted.
                    historyNamespace = m_PendingHistoryReleases.front().HistoryNamespace;
                    m_PendingHistoryReleases.pop_front();
                }
                else if (!stopping && !m_PendingHistoryReleases.empty() &&
                         m_PendingHistoryReleases.front().AfterSubmissionValue <= m_LastCompletedSubmissionValue)
                {
                    historyNamespace = m_PendingHistoryReleases.front().HistoryNamespace;
                    m_PendingHistoryReleases.pop_front();
                }
                else
                {
                    contextIndex = m_ReadyContexts.front();
                    m_ReadyContexts.pop_front();
                    m_ContextStates[contextIndex] = ContextState::Rendering;
                    m_RenderingContexts++;
                }
            }

            if (historyNamespace != 0)
            {
                SceneRenderer::ReleaseRenderThreadHistory(historyNamespace);
                continue;
            }

            FrameContext& context = m_FrameContexts[contextIndex];

            FrameMarkStart("RenderThread");

            {
                ZoneScopedN("DrainMeshUploads");
                for (MeshUploadCommand& command : context.MeshUploadCommands)
                    command.Execute();
                context.MeshUploadCommands.clear();
            }

            {
                ZoneScopedN("RenderFromSnapshot");
                SceneRenderer::RenderFromSnapshot(context.Snapshot);
            }

            FrameMarkEnd("RenderThread");

            {
                ScopedLock lock(m_ContextMutex);
                context.CompletionValue = context.SubmissionValue;
                m_LastCompletedSubmissionValue = context.CompletionValue;
                m_ContextStates[contextIndex] = ContextState::Available;
                m_RenderingContexts--;
            }
            m_ContextAvailable.notify_all();
        }

        Vector<MeshUploadCommand> pendingMeshUploads;
        {
            ScopedLock lock(m_MeshUploadMutex);
            pendingMeshUploads = std::move(m_PendingMeshUploads);
            m_PendingMeshUploads.clear();
        }
        for (MeshUploadCommand& command : pendingMeshUploads)
            command.Execute();

        SceneRenderer::ShutdownRenderThreadResources();
    }

} // namespace Crowny

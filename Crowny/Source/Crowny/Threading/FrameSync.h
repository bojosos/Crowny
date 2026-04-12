#pragma once

#include "Crowny/Common/StdHeaders.h"

namespace Crowny
{

    // Synchronizes a sim thread and a render thread so that the sim thread
    // is at most 1 frame ahead of the render thread.
    //
    // Protocol per frame:
    //   Sim thread:    SimWaitForRenderDone() -> [extract snapshot] -> SimSignalNewFrame()
    //   Render thread: RenderWaitForNewFrame() -> [render] -> RenderSignalDone()
    class FrameSync
    {
    public:
        // Blocks the sim thread until the render thread has finished the previous frame.
        void SimWaitForRenderDone()
        {
            Lock lock(m_Mutex);
            while (!m_FrameDone && m_Running)
            {
                m_RenderDone.wait(lock);
            }
        }

        // Signals the render thread that a new snapshot is ready.
        void SimSignalNewFrame()
        {
            {
                Lock lock(m_Mutex);
                m_NewFrameReady = true;
                m_FrameDone = false;
            }
            m_RenderHasWork.notify_one();
        }

        // Blocks the render thread until the sim thread provides a new snapshot.
        // Returns false if shutdown was requested.
        bool RenderWaitForNewFrame()
        {
            Lock lock(m_Mutex);
            while (!m_NewFrameReady && m_Running)
            {
                m_RenderHasWork.wait(lock);
            }
            m_NewFrameReady = false;
            return m_Running;
        }

        // Signals the sim thread that rendering is complete.
        void RenderSignalDone()
        {
            {
                Lock lock(m_Mutex);
                m_FrameDone = true;
            }
            m_RenderDone.notify_one();
        }

        // Wake both threads for clean shutdown.
        void Shutdown()
        {
            {
                Lock lock(m_Mutex);
                m_Running = false;
                m_FrameDone = true;
                m_NewFrameReady = true;
            }
            m_RenderHasWork.notify_all();
            m_RenderDone.notify_all();
        }

        bool IsRunning() const { return m_Running; }

    private:
        Mutex m_Mutex;
        Signal m_RenderHasWork;
        Signal m_RenderDone;
        bool m_NewFrameReady = false;
        bool m_FrameDone = true;
        bool m_Running = true;
    };

} // namespace Crowny

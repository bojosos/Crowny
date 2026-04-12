#pragma once

#include "Crowny/Common/StdHeaders.h"

#include <functional>

namespace Crowny
{

    // Single-producer single-consumer command queue with double buffering.
    // Sim thread enqueues commands into the write buffer.
    // At frame boundary, Swap() exchanges write and read buffers.
    // Render thread drains the read buffer.
    class CommandQueue
    {
    public:
        void Enqueue(std::function<void()>&& cmd)
        {
            Lock lock(m_SwapMutex);
            m_Queues[m_WriteIdx].push_back(std::move(cmd));
        }

        // Called at the frame sync point to swap write and read buffers.
        void Swap()
        {
            Lock lock(m_SwapMutex);
            m_WriteIdx ^= 1;
        }

        // Called by the consumer (render thread) to execute all queued commands.
        void DrainAndExecute()
        {
            uint32_t readIdx;
            {
                Lock lock(m_SwapMutex);
                readIdx = m_WriteIdx ^ 1;
            }

            auto& queue = m_Queues[readIdx];
            for (size_t i = 0; i < queue.size(); i++)
            {
                queue[i]();
            }
            queue.clear();
        }

    private:
        Vector<std::function<void()>> m_Queues[2];
        uint32_t m_WriteIdx = 0;
        Mutex m_SwapMutex;
    };

} // namespace Crowny

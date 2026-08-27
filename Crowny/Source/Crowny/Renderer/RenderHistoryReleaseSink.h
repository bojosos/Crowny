#pragma once

#include <cstdint>

namespace Crowny
{
    class RenderHistoryReleaseSink
    {
    public:
        virtual ~RenderHistoryReleaseSink() = default;

        // The sink owns ordering and thread affinity after this call returns.
        virtual void QueueHistoryRelease(uint64_t historyNamespace) = 0;
    };
} // namespace Crowny

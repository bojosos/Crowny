#pragma once

#include "Crowny/Renderer/RenderSnapshot.h"

#include <functional>

namespace Crowny
{
    struct FrameContext
    {
        explicit FrameContext(uint32_t index = 0) : Index(index) { ResourceCommands.reserve(64); }

        void BeginRecording(uint64_t frameNumber)
        {
            Snapshot.Clear();
            Snapshot.FrameNumber = frameNumber;
            ResourceCommands.clear();
        }

        uint32_t Index = 0;
        uint64_t SubmissionValue = 0;
        uint64_t CompletionValue = 0;
        RenderSnapshot Snapshot;
        Vector<std::function<void()>> ResourceCommands;
    };

} // namespace Crowny

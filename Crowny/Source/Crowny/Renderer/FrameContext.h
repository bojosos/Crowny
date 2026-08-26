#pragma once

#include "Crowny/Renderer/MeshUploadCommand.h"
#include "Crowny/Renderer/RenderSnapshot.h"

namespace Crowny
{
    struct FrameContext
    {
        explicit FrameContext(uint32_t index = 0) : Index(index) { MeshUploadCommands.reserve(64); }

        void BeginRecording(uint64_t frameNumber)
        {
            Snapshot.Clear();
            Snapshot.FrameNumber = frameNumber;
            MeshUploadCommands.clear();
        }

        uint32_t Index = 0;
        uint64_t SubmissionValue = 0;
        uint64_t CompletionValue = 0;
        RenderSnapshot Snapshot;
        Vector<MeshUploadCommand> MeshUploadCommands;
    };

} // namespace Crowny

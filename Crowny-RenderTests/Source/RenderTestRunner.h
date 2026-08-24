#pragma once

#include "Crowny/Common/StdHeaders.h"
#include "Crowny/RenderAPI/RenderAPI.h"

namespace Crowny::RenderTests
{
    struct RunnerOptions
    {
        RenderAPI::API Backend = RenderAPI::API::Vulkan;
        Path References = "Crowny-RenderTests/References";
        Path Artifacts = "artifacts/render-tests";
        String Filter;
        bool UpdateReferences = false;
        bool ShowHelp = false;
    };

    bool ParseOptions(int argc, char** argv, RunnerOptions& options, String& error);
    void PrintUsage();
    int RunSuite(const RunnerOptions& options);
    int CompareBackendDirectories(const Path& first, const Path& second, const Path& artifacts);
} // namespace Crowny::RenderTests

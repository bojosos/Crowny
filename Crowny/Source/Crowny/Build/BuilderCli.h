#pragma once

#include "Crowny/Build/BuildPipeline.h"

#include <iosfwd>

namespace Crowny
{
    enum class BuilderExitCode : int
    {
        Success = 0,
        InvalidCommandLine = 2,
        InputError = 3,
        BuildFailed = 4,
        Cancelled = 5,
        InternalError = 70
    };

    int RunCrownyBuilder(const Vector<String>& arguments, std::ostream& output, std::ostream& error,
                         BuildCancellationCheck cancellation = {});
} // namespace Crowny

#pragma once

#include "Crowny/Build/BuildTypes.h"
#include "Crowny/Scripting/Managed/ManagedTypes.h"

namespace Crowny
{
    struct ManagedBackendSelection
    {
        ManagedScriptingConfig Runtime;
        bool ClosedWorld = false;
        bool SupportsProgramReload = false;
        bool RequiresGeneratedMetadata = false;
    };

    struct ManagedBackendAvailability
    {
        bool Available = false;
        String Reason;
    };

    ManagedBackendSelection ResolveManagedBackendPreset(ManagedBackendPreset preset);
    ManagedBackendAvailability GetManagedBackendAvailability(ManagedBackendPreset preset, BuildPlatform platform,
                                                              BuildConfiguration configuration, bool editor);
} // namespace Crowny

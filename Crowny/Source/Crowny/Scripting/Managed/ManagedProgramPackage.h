#pragma once

#include "Crowny/Scripting/Managed/ManagedTypes.h"

namespace Crowny
{
    struct ManagedProgramPackage
    {
        ManagedScriptingConfig Runtime;
        ManagedProgramDefinition Program;
    };

    struct ManagedProgramPackageResult
    {
        ManagedOperationResult Result;
        ManagedProgramPackage Package;
    };

    ManagedProgramPackageResult LoadManagedProgramPackage(const Path& manifestPath, uint64_t generation = 1);
} // namespace Crowny

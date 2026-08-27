#pragma once

#include "Crowny/Scripting/Managed/ManagedTypes.h"

namespace Crowny
{
    struct PersistedScriptState;

    // Transitional compatibility bridge for scene data written by the legacy
    // Mono serializer. The returned state contains no runtime-specific types.
    ScriptState ConvertLegacyScriptState(const PersistedScriptState& persisted);
} // namespace Crowny

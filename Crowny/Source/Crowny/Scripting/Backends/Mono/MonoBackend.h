#pragma once

#include "Crowny/Scripting/Managed/Internal/ManagedBackend.h"

namespace Crowny
{
    class Entity;
    struct AssemblyRefreshResult;

    namespace MonoBackendDetail
    {
        void RollbackAddedScriptOccurrence(Entity entity, uint64_t runtimeInstanceId, bool occurrenceAdded, bool componentAdded);
        ManagedBackendReloadResult BuildAssemblyRefreshFailure(const AssemblyRefreshResult& refresh);
        ManagedBackendReloadResult AddReloadRollbackDiagnostics(ManagedOperationResult failure, bool assembliesRestored,
                                                                const ManagedOperationResult& stateRestoration);
    } // namespace MonoBackendDetail
} // namespace Crowny

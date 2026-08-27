#pragma once

#include "Crowny/Scripting/Managed/Internal/ManagedBackend.h"

namespace Crowny
{
    class Entity;
    class SerializableMemberInfo;

    namespace MonoBackendDetail
    {
        ScriptSchemaFieldFlags GetSchemaFieldFlags(const Ref<SerializableMemberInfo>& member);
        void RollbackAddedScriptOccurrence(Entity entity, uint64_t runtimeInstanceId, bool occurrenceAdded, bool componentAdded);
        ManagedOperationResult AddReloadRollbackDiagnostics(ManagedOperationResult failure, bool assembliesRestored,
                                                            const ManagedOperationResult& stateRestoration);
    } // namespace MonoBackendDetail
} // namespace Crowny

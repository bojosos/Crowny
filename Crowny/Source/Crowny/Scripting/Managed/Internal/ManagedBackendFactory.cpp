#include "cwpch.h"

#include "Crowny/Scripting/Managed/Internal/ManagedBackend.h"

namespace Crowny
{
    Scope<ManagedBackend> CreateManagedBackend(ManagedBackendId backend)
    {
        switch (backend)
        {
        case ManagedBackendId::CoreCLR: return CreateCoreClrBackend();
        case ManagedBackendId::GeneratedMetadata: return CreateGeneratedMetadataBackend();
        case ManagedBackendId::Mono:
        case ManagedBackendId::DotNetWasm:
        case ManagedBackendId::NativeAOT: return nullptr;
        }
        return nullptr;
    }
} // namespace Crowny

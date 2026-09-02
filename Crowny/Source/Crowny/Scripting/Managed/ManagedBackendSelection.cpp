#include "cwpch.h"

#include "Crowny/Scripting/Managed/ManagedBackendSelection.h"

namespace Crowny
{
    ManagedBackendSelection ResolveManagedBackendPreset(ManagedBackendPreset preset)
    {
        ManagedBackendSelection selection;
        switch (preset)
        {
        case ManagedBackendPreset::Mono:
            selection.Runtime.Backend = ManagedBackendId::Mono;
            selection.Runtime.ExecutionMode = ManagedExecutionMode::Interpreter;
            selection.SupportsProgramReload = true;
            break;
        case ManagedBackendPreset::CoreCLR:
            selection.Runtime.Backend = ManagedBackendId::CoreCLR;
            selection.Runtime.ExecutionMode = ManagedExecutionMode::Jit;
            selection.SupportsProgramReload = true;
            break;
        case ManagedBackendPreset::DotNetWasmInterpreter:
            selection.Runtime.Backend = ManagedBackendId::DotNetWasm;
            selection.Runtime.ExecutionMode = ManagedExecutionMode::Interpreter;
            selection.RequiresGeneratedMetadata = true;
            break;
        case ManagedBackendPreset::DotNetWasmAOT:
            selection.Runtime.Backend = ManagedBackendId::DotNetWasm;
            selection.Runtime.ExecutionMode = ManagedExecutionMode::Aot;
            selection.ClosedWorld = true;
            selection.RequiresGeneratedMetadata = true;
            break;
        case ManagedBackendPreset::NativeAOT:
            selection.Runtime.Backend = ManagedBackendId::NativeAOT;
            selection.Runtime.ExecutionMode = ManagedExecutionMode::Aot;
            selection.ClosedWorld = true;
            selection.RequiresGeneratedMetadata = true;
            break;
        }
        return selection;
    }

    ManagedBackendAvailability GetManagedBackendAvailability(ManagedBackendPreset preset, CW_MAYBE_UNUSED BuildPlatform platform,
                                                              BuildConfiguration configuration, bool editor)
    {
        if (editor)
        {
            if (preset == ManagedBackendPreset::Mono || preset == ManagedBackendPreset::CoreCLR)
                return { true, {} };
            return { false, "The editor requires a reloadable Mono or CoreCLR backend." };
        }

        const bool webBackend = preset == ManagedBackendPreset::DotNetWasmInterpreter || preset == ManagedBackendPreset::DotNetWasmAOT;
        if (webBackend)
            return { false, ".NET WebAssembly backends require a Web build target, which is not available yet." };
        if (preset == ManagedBackendPreset::NativeAOT && configuration != BuildConfiguration::Shipping)
            return { false, "Native AOT is a closed-world shipping backend and is unavailable for development players." };
        return { true, {} };
    }
} // namespace Crowny

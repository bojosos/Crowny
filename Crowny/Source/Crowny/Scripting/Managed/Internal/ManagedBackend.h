#pragma once

#include "Crowny/Scripting/Managed/ManagedTypes.h"

namespace Crowny
{
    struct ManagedBackendCreateResult
    {
        ManagedOperationResult Result;
        uint64_t Handle = 0;
    };

    struct ManagedBackendStateResult
    {
        ManagedOperationResult Result;
        ScriptState State;
    };

    struct ManagedBackendReloadInstance
    {
        uint64_t PreviousHandle = 0;
        UUID Entity;
        ScriptState State;
    };

    struct ManagedBackendReloadResult
    {
        ManagedOperationResult Result;
        Vector<uint64_t> ReplacementHandles;
        bool ProgramInvalidated = false;
    };

    class ManagedBackend
    {
    public:
        virtual ~ManagedBackend() = default;

        virtual ManagedOperationResult Start(const ManagedScriptingConfig& config) = 0;
        virtual void Shutdown() = 0;
        virtual ManagedCapabilities GetCapabilities() const = 0;
        virtual ManagedOperationResult LoadProgram(const ManagedProgramDefinition& program) = 0;
        virtual ManagedBackendReloadResult ReloadProgram(const ManagedProgramDefinition& program,
                                                         const Vector<ManagedBackendReloadInstance>& instances) = 0;
        virtual const ScriptCatalog& GetScriptCatalog() const = 0;
        virtual ManagedBackendCreateResult CreateScript(const ScriptCreateRequest& request) = 0;
        virtual ManagedOperationResult DestroyScript(uint64_t handle) = 0;
        virtual ManagedOperationResult Dispatch(uint64_t handle, const ScriptEvent& event) = 0;
        virtual ManagedBackendStateResult CaptureState(uint64_t handle) = 0;
        virtual ManagedOperationResult ApplyState(uint64_t handle, const ScriptState& state) = 0;
        virtual Vector<ManagedDiagnostic> Update() = 0;
    };

    Scope<ManagedBackend> CreateManagedBackend(ManagedBackendId backend);
    Scope<ManagedBackend> CreateMonoBackend();
    Scope<ManagedBackend> CreateCoreClrBackend();
    Scope<ManagedBackend> CreateGeneratedMetadataBackend();
} // namespace Crowny

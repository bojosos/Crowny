#pragma once

#include "Crowny/Scripting/Managed/ManagedTypes.h"

namespace Crowny
{
    class Entity;
    class ManagedBackend;
    class Scene;

    class ManagedScripting
    {
    public:
        ManagedScripting();
        ~ManagedScripting();

        ManagedScripting(const ManagedScripting&) = delete;
        ManagedScripting& operator=(const ManagedScripting&) = delete;

        ManagedOperationResult Start(const ManagedScriptingConfig& config);
        void Shutdown();
        bool IsStarted() const { return m_Backend != nullptr; }

        const ManagedCapabilities& GetCapabilities() const { return m_Capabilities; }
        const ScriptCatalog& GetScriptCatalog() const;

        ManagedOperationResult LoadProgram(const ManagedProgramDefinition& program);
        ManagedOperationResult ReloadProgram(const ManagedProgramDefinition& program);
        ScriptCreateResult CreateScript(const ScriptCreateRequest& request);
        ManagedOperationResult DestroyScript(ScriptInstanceHandle handle);
        ManagedOperationResult Dispatch(ScriptInstanceHandle handle, const ScriptEvent& event);
        ScriptStateResult CaptureState(ScriptInstanceHandle handle);
        ManagedOperationResult ApplyState(ScriptInstanceHandle handle, const ScriptState& state);
        ScriptInvocationResult InvokeButton(ScriptInstanceHandle handle, uint64_t methodId, const Vector<ScriptValue>& arguments);
        Vector<ManagedDiagnostic> Update();
        void NotifyEntityDestroyed(const Entity& entity);
        void NotifyComponentDestroyed(uint64_t instanceId);
        void NotifySceneDestroyed(const Scene* scene);
        void NotifySceneEventsAvailable();

    private:
        struct InstanceSlot
        {
            uint64_t BackendHandle = 0;
            UUID Entity;
            uint32_t Generation = 1;
            bool Active = false;
        };

        ManagedOperationResult RequireStarted() const;
        InstanceSlot* Resolve(ScriptInstanceHandle handle);
        const InstanceSlot* Resolve(ScriptInstanceHandle handle) const;
        ScriptInstanceHandle AllocateHandle(uint64_t backendHandle, const UUID& entity);
        void InvalidateInstanceSlots();
        static ManagedOperationResult StaleHandle(ManagedBackendId backend);

        Scope<ManagedBackend> m_Backend;
        ManagedScriptingConfig m_Config;
        ManagedCapabilities m_Capabilities;
        Vector<InstanceSlot> m_Instances;
    };
} // namespace Crowny

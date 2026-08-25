#include "cwpch.h"

#include "Crowny/Scripting/Managed/ManagedScripting.h"
#include "Crowny/Scripting/Managed/Internal/ManagedBackend.h"

namespace Crowny
{
    namespace
    {
        const ScriptCatalog EMPTY_CATALOG;

        uint32_t NextGeneration(uint32_t generation)
        {
            ++generation;
            return generation == 0 ? 1 : generation;
        }
    } // namespace

    ManagedScripting::ManagedScripting() { m_Instances.resize(1); }

    ManagedScripting::~ManagedScripting() { Shutdown(); }

    ManagedOperationResult ManagedScripting::Start(const ManagedScriptingConfig& config)
    {
        if (IsStarted())
            return ManagedOperationResult::Failure("managed.module.already_started", "Managed scripting is already running.", config.Backend);

        Scope<ManagedBackend> backend = CreateManagedBackend(config.Backend);
        if (backend == nullptr)
            return ManagedOperationResult::Failure("managed.backend.unavailable",
                                                   String("Managed backend ") + ToString(config.Backend) + " is not available in this build.",
                                                   config.Backend);

        ManagedOperationResult result = backend->Start(config);
        if (!result.Succeeded)
            return result;

        m_Config = config;
        m_Capabilities = backend->GetCapabilities();
        m_Backend = std::move(backend);
        return result;
    }

    void ManagedScripting::Shutdown()
    {
        if (!IsStarted())
            return;

        for (size_t index = 1; index < m_Instances.size(); ++index)
        {
            InstanceSlot& slot = m_Instances[index];
            if (!slot.Active)
                continue;
            m_Backend->DestroyScript(slot.BackendHandle);
            slot.Active = false;
            slot.BackendHandle = 0;
            slot.Entity = {};
            slot.Generation = NextGeneration(slot.Generation);
        }

        m_Backend->Shutdown();
        m_Backend = nullptr;
        m_Capabilities = {};
    }

    const ScriptCatalog& ManagedScripting::GetScriptCatalog() const
    {
        return IsStarted() ? m_Backend->GetScriptCatalog() : EMPTY_CATALOG;
    }

    ManagedOperationResult ManagedScripting::LoadProgram(const ManagedProgramDefinition& program)
    {
        if (ManagedOperationResult ready = RequireStarted(); !ready.Succeeded)
            return ready;
        if (program.Generation == 0)
            return ManagedOperationResult::Failure("managed.program.generation_invalid",
                                                   "A managed program generation must be nonzero.", m_Config.Backend);
        if (std::any_of(m_Instances.begin() + 1, m_Instances.end(), [](const InstanceSlot& slot) { return slot.Active; }))
            return ManagedOperationResult::Failure("managed.program.instances_active",
                                                   "Destroy live script instances before loading a different managed program.", m_Config.Backend);
        return m_Backend->LoadProgram(program);
    }

    ManagedOperationResult ManagedScripting::ReloadProgram(const ManagedProgramDefinition& program)
    {
        if (ManagedOperationResult ready = RequireStarted(); !ready.Succeeded)
            return ready;
        if (!m_Capabilities.Reload)
            return ManagedOperationResult::Failure("managed.capability.reload_unavailable",
                                                   String("Managed backend ") + ToString(m_Config.Backend) + " does not support program reload.",
                                                   m_Config.Backend);
        if (program.Generation == 0)
            return ManagedOperationResult::Failure("managed.program.generation_invalid",
                                                   "A managed program generation must be nonzero.", m_Config.Backend);

        Vector<ManagedBackendReloadInstance> instances;
        Vector<size_t> publicSlots;
        for (size_t index = 1; index < m_Instances.size(); ++index)
        {
            const InstanceSlot& slot = m_Instances[index];
            if (!slot.Active)
                continue;
            ManagedBackendStateResult captured = m_Backend->CaptureState(slot.BackendHandle);
            if (!captured.Result.Succeeded)
                return captured.Result;
            instances.push_back({ slot.BackendHandle, slot.Entity, std::move(captured.State) });
            publicSlots.push_back(index);
        }

        ManagedBackendReloadResult result = m_Backend->ReloadProgram(program, instances);
        if (!result.Result.Succeeded)
            return result.Result;
        if (result.ReplacementHandles.size() != publicSlots.size())
            return ManagedOperationResult::Failure("managed.reload.handle_count_mismatch",
                                                   "The backend returned an invalid replacement-handle count.", m_Config.Backend);
        if (std::any_of(result.ReplacementHandles.begin(), result.ReplacementHandles.end(), [](uint64_t handle) { return handle == 0; }))
            return ManagedOperationResult::Failure("managed.reload.handle_invalid",
                                                   "The backend returned an invalid replacement handle.", m_Config.Backend);
        for (size_t index = 0; index < publicSlots.size(); ++index)
            m_Instances[publicSlots[index]].BackendHandle = result.ReplacementHandles[index];
        return result.Result;
    }

    ScriptCreateResult ManagedScripting::CreateScript(const ScriptCreateRequest& request)
    {
        if (ManagedOperationResult ready = RequireStarted(); !ready.Succeeded)
            return { std::move(ready), {} };
        if (!request.Identity.IsValid())
            return { ManagedOperationResult::Failure("managed.script.identity_invalid", "Script identity is incomplete.", m_Config.Backend), {} };

        ManagedBackendCreateResult backendResult = m_Backend->CreateScript(request);
        if (!backendResult.Result.Succeeded)
            return { std::move(backendResult.Result), {} };
        if (backendResult.Handle == 0)
            return { ManagedOperationResult::Failure("managed.backend.handle_invalid",
                                                     "The managed backend returned an invalid script handle.", m_Config.Backend),
                     {} };
        return { std::move(backendResult.Result), AllocateHandle(backendResult.Handle, request.Entity) };
    }

    ManagedOperationResult ManagedScripting::DestroyScript(ScriptInstanceHandle handle)
    {
        if (ManagedOperationResult ready = RequireStarted(); !ready.Succeeded)
            return ready;
        InstanceSlot* slot = Resolve(handle);
        if (slot == nullptr)
            return StaleHandle(m_Config.Backend);

        ManagedOperationResult result = m_Backend->DestroyScript(slot->BackendHandle);
        if (!result.Succeeded)
            return result;
        slot->Active = false;
        slot->BackendHandle = 0;
        slot->Entity = {};
        slot->Generation = NextGeneration(slot->Generation);
        return result;
    }

    ManagedOperationResult ManagedScripting::Dispatch(ScriptInstanceHandle handle, const ScriptEvent& event)
    {
        if (ManagedOperationResult ready = RequireStarted(); !ready.Succeeded)
            return ready;
        const InstanceSlot* slot = Resolve(handle);
        return slot != nullptr ? m_Backend->Dispatch(slot->BackendHandle, event) : StaleHandle(m_Config.Backend);
    }

    ScriptStateResult ManagedScripting::CaptureState(ScriptInstanceHandle handle)
    {
        if (ManagedOperationResult ready = RequireStarted(); !ready.Succeeded)
            return { std::move(ready), {} };
        const InstanceSlot* slot = Resolve(handle);
        if (slot == nullptr)
            return { StaleHandle(m_Config.Backend), {} };

        ManagedBackendStateResult result = m_Backend->CaptureState(slot->BackendHandle);
        return { std::move(result.Result), std::move(result.State) };
    }

    ManagedOperationResult ManagedScripting::ApplyState(ScriptInstanceHandle handle, const ScriptState& state)
    {
        if (ManagedOperationResult ready = RequireStarted(); !ready.Succeeded)
            return ready;
        const InstanceSlot* slot = Resolve(handle);
        return slot != nullptr ? m_Backend->ApplyState(slot->BackendHandle, state) : StaleHandle(m_Config.Backend);
    }

    Vector<ManagedDiagnostic> ManagedScripting::Update() { return IsStarted() ? m_Backend->Update() : Vector<ManagedDiagnostic>{}; }

    ManagedOperationResult ManagedScripting::RequireStarted() const
    {
        return IsStarted() ? ManagedOperationResult::Success()
                           : ManagedOperationResult::Failure("managed.module.not_started", "Managed scripting is not running.",
                                                             m_Config.Backend);
    }

    ManagedScripting::InstanceSlot* ManagedScripting::Resolve(ScriptInstanceHandle handle)
    {
        return const_cast<InstanceSlot*>(static_cast<const ManagedScripting*>(this)->Resolve(handle));
    }

    const ManagedScripting::InstanceSlot* ManagedScripting::Resolve(ScriptInstanceHandle handle) const
    {
        const uint32_t slotIndex = static_cast<uint32_t>(handle.m_Value & 0xFFFFFFFFull);
        const uint32_t generation = static_cast<uint32_t>(handle.m_Value >> 32u);
        if (slotIndex == 0 || slotIndex >= m_Instances.size())
            return nullptr;
        const InstanceSlot& slot = m_Instances[slotIndex];
        return slot.Active && slot.Generation == generation ? &slot : nullptr;
    }

    ScriptInstanceHandle ManagedScripting::AllocateHandle(uint64_t backendHandle, const UUID& entity)
    {
        size_t slotIndex = 1;
        for (; slotIndex < m_Instances.size(); ++slotIndex)
        {
            if (!m_Instances[slotIndex].Active)
                break;
        }
        if (slotIndex == m_Instances.size())
            m_Instances.push_back({});

        InstanceSlot& slot = m_Instances[slotIndex];
        slot.BackendHandle = backendHandle;
        slot.Entity = entity;
        slot.Active = true;
        const uint64_t value = (static_cast<uint64_t>(slot.Generation) << 32u) | static_cast<uint64_t>(slotIndex);
        return ScriptInstanceHandle(value);
    }

    ManagedOperationResult ManagedScripting::StaleHandle(ManagedBackendId backend)
    {
        return ManagedOperationResult::Failure("managed.instance.stale_handle", "The script instance handle is stale or invalid.", backend);
    }
} // namespace Crowny

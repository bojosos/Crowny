#include "cwpch.h"

#include "Crowny/Scripting/Managed/Internal/ManagedBackend.h"

namespace Crowny
{
    namespace
    {
        class GeneratedMetadataBackend final : public ManagedBackend
        {
        public:
            ManagedOperationResult Start(const ManagedScriptingConfig& config) override
            {
                if (config.ExecutionMode != ManagedExecutionMode::Aot)
                    return ManagedOperationResult::Failure("managed.generated.execution_mode",
                                                           "The generated metadata backend requires AOT execution mode.",
                                                           ManagedBackendId::GeneratedMetadata);
                m_Started = true;
                return ManagedOperationResult::Success();
            }

            void Shutdown() override
            {
                m_Instances.clear();
                m_Catalog = {};
                m_NextHandle = 1;
                m_Started = false;
            }

            ManagedCapabilities GetCapabilities() const override
            {
                ManagedCapabilities capabilities;
                capabilities.DynamicProgramLoading = true;
                capabilities.AotOnly = true;
                return capabilities;
            }

            ManagedOperationResult LoadProgram(const ManagedProgramDefinition& program) override
            {
                if (!m_Started)
                    return NotStarted();
                if (!m_Instances.empty())
                    return ManagedOperationResult::Failure("managed.generated.instances_active",
                                                           "Generated metadata instances are still active.",
                                                           ManagedBackendId::GeneratedMetadata);
                ManagedOperationResult validation = ValidateScriptCatalog(program.Catalog, ManagedBackendId::GeneratedMetadata);
                if (!validation.Succeeded)
                    return validation;
                m_Catalog = program.Catalog;
                return ManagedOperationResult::Success();
            }

            ManagedBackendReloadResult ReloadProgram(const ManagedProgramDefinition&,
                                                     const Vector<ManagedBackendReloadInstance>&) override
            {
                return { ManagedOperationResult::Failure("managed.capability.reload_unavailable",
                                                         "The generated metadata backend does not support reload.",
                                                         ManagedBackendId::GeneratedMetadata),
                         {} };
            }

            const ScriptCatalog& GetScriptCatalog() const override { return m_Catalog; }

            ManagedBackendCreateResult CreateScript(const ScriptCreateRequest& request) override
            {
                if (!m_Started)
                    return { NotStarted(), 0 };
                if (m_Catalog.FindType(request.Identity) == nullptr)
                    return { ManagedOperationResult::Failure("managed.script.type_missing",
                                                             "The script type is not present in the generated catalog.",
                                                             ManagedBackendId::GeneratedMetadata),
                             0 };
                const ScriptTypeSchema* schema = m_Catalog.FindType(request.Identity);
                ScriptStateResult normalized = NormalizeScriptState(request.InitialState, *schema, ManagedBackendId::GeneratedMetadata);
                if (!normalized.Result.Succeeded)
                    return { std::move(normalized.Result), 0 };

                if (m_NextHandle == 0)
                    return { ManagedOperationResult::Failure("managed.generated.handle_exhausted",
                                                             "Generated metadata script handles are exhausted.",
                                                             ManagedBackendId::GeneratedMetadata),
                             0 };
                const uint64_t handle = m_NextHandle++;
                m_Instances.emplace(handle, Instance{ request.Entity, std::move(normalized.State) });
                return { ManagedOperationResult::Success(), handle };
            }

            ManagedOperationResult DestroyScript(uint64_t handle) override
            {
                if (m_Instances.erase(handle) == 0)
                    return StaleHandle();
                return ManagedOperationResult::Success();
            }

            ManagedOperationResult Dispatch(uint64_t handle, const ScriptEvent&) override
            {
                return m_Instances.find(handle) != m_Instances.end() ? ManagedOperationResult::Success() : StaleHandle();
            }

            ManagedBackendStateResult CaptureState(uint64_t handle) override
            {
                const auto instance = m_Instances.find(handle);
                return instance != m_Instances.end() ? ManagedBackendStateResult{ ManagedOperationResult::Success(), instance->second.State }
                                                     : ManagedBackendStateResult{ StaleHandle(), {} };
            }

            ManagedOperationResult ApplyState(uint64_t handle, const ScriptState& state) override
            {
                const auto instance = m_Instances.find(handle);
                if (instance == m_Instances.end())
                    return StaleHandle();
                const ScriptTypeSchema* schema = m_Catalog.FindType(instance->second.State.Identity);
                ScriptStateResult normalized = NormalizeScriptState(state, *schema, ManagedBackendId::GeneratedMetadata);
                if (!normalized.Result.Succeeded)
                    return normalized.Result;
                instance->second.State = std::move(normalized.State);
                return normalized.Result;
            }

            Vector<ManagedDiagnostic> Update() override { return {}; }

        private:
            struct Instance
            {
                UUID Entity;
                ScriptState State;
            };

            static ManagedOperationResult NotStarted()
            {
                return ManagedOperationResult::Failure("managed.backend.not_started",
                                                       "The generated metadata backend is not running.",
                                                       ManagedBackendId::GeneratedMetadata);
            }

            static ManagedOperationResult StaleHandle()
            {
                return ManagedOperationResult::Failure("managed.backend.stale_handle",
                                                       "The generated metadata instance handle is stale.",
                                                       ManagedBackendId::GeneratedMetadata);
            }

            bool m_Started = false;
            uint64_t m_NextHandle = 1;
            ScriptCatalog m_Catalog;
            Map<uint64_t, Instance> m_Instances;
        };
    } // namespace

    Scope<ManagedBackend> CreateGeneratedMetadataBackend() { return CreateScope<GeneratedMetadataBackend>(); }
} // namespace Crowny

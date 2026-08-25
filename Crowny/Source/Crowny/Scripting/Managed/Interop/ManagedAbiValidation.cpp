#include "cwpch.h"

#include "Crowny/Scripting/Managed/Interop/ManagedAbiValidation.h"

namespace Crowny
{
    ManagedOperationResult ValidateManagedProgramApi(const cw_managed_program_api& api, ManagedBackendId backend)
    {
        if (api.size < sizeof(cw_managed_program_api) || api.abi_version != CW_MANAGED_ABI_VERSION)
            return ManagedOperationResult::Failure("managed.abi.version_mismatch",
                                                   "The managed host and native engine use incompatible ABI versions.", backend);
        if (api.initialize == nullptr || api.shutdown == nullptr || api.load_program == nullptr || api.unload_program == nullptr ||
            api.get_catalog == nullptr || api.create_script == nullptr || api.destroy_script == nullptr || api.dispatch == nullptr ||
            api.capture_state == nullptr || api.apply_state == nullptr || api.collect_diagnostics == nullptr)
            return ManagedOperationResult::Failure("managed.abi.entrypoint_missing",
                                                   "The managed host did not provide every required ABI entry point.", backend);
        return ManagedOperationResult::Success();
    }
} // namespace Crowny

#pragma once

#include "Crowny/Scripting/Managed/Interop/CrownyManagedAbi.h"
#include "Crowny/Scripting/Managed/ManagedTypes.h"

namespace Crowny
{
    ManagedOperationResult ValidateManagedProgramApi(const cw_managed_program_api& api, ManagedBackendId backend);
} // namespace Crowny

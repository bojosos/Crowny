#pragma once

#include "Crowny/Scripting/Managed/Interop/CrownyManagedAbi.h"

namespace Crowny
{
    void PopulateManagedHostBindings(cw_managed_host_api& api);
    void ReleaseManagedHostBindings(void* context);
} // namespace Crowny

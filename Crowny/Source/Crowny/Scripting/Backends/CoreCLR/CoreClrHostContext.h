#pragma once

#include "Crowny/Scripting/Managed/Interop/CrownyManagedAbi.h"
#include "Crowny/Scripting/Managed/ManagedTypes.h"

namespace Crowny
{
    struct CoreClrHostContextState;

    // Owns a revocable host table. Copies retained by managed code contain an opaque token, never a backend pointer.
    class CoreClrHostContext
    {
    public:
        CoreClrHostContext();
        explicit CoreClrHostContext(const cw_managed_host_api& bindings);
        ~CoreClrHostContext();
        CoreClrHostContext(const CoreClrHostContext&) = delete;
        CoreClrHostContext& operator=(const CoreClrHostContext&) = delete;

        const cw_managed_host_api& GetApi() const { return m_Api; }
        void Revoke();
        Vector<ManagedDiagnostic> TakeDiagnostics();

    private:
        Ref<CoreClrHostContextState> m_State;
        cw_managed_host_api m_Api{};
    };
} // namespace Crowny

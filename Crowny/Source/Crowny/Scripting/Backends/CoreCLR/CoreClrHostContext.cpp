#include "cwpch.h"

#include "Crowny/Scripting/Backends/CoreCLR/CoreClrHostContext.h"
#include "Crowny/Scripting/Managed/Interop/ManagedHostBindings.h"

#include <stdexcept>
#include <type_traits>

namespace Crowny
{
    struct CoreClrHostContextState : RefCounted
    {
        std::recursive_mutex Mutex;
        std::mutex ReleaseMutex;
        std::mutex DiagnosticMutex;
        const std::thread::id OwnerThread = std::this_thread::get_id();
        std::atomic<bool> Active = true;
        cw_managed_host_api Bindings{};
        Vector<ManagedDiagnostic> Diagnostics;
    };

    namespace
    {
        struct HostContextRegistry
        {
            Mutex Guard;
            uintptr_t NextToken = 1;
            UnorderedMap<void*, Ref<CoreClrHostContextState>> Contexts;
        };

        HostContextRegistry& Registry()
        {
            static HostContextRegistry registry;
            return registry;
        }

        Ref<CoreClrHostContextState> FindContext(void* token)
        {
            HostContextRegistry& registry = Registry();
            Lock lock(registry.Guard);
            const auto found = registry.Contexts.find(token);
            return found != registry.Contexts.end() ? found->second : nullptr;
        }

        const cw_managed_host_api& NativeApi()
        {
            static const cw_managed_host_api api = [] {
                cw_managed_host_api value{};
                PopulateManagedHostBindings(value);
                return value;
            }();
            return api;
        }

        template <auto Member, typename Signature> struct GuardedHostFunction;

        template <auto Member, typename... Args> struct GuardedHostFunction<Member, cw_managed_status(CW_MANAGED_CALL*)(void*, Args...)>
        {
            static cw_managed_status CW_MANAGED_CALL Invoke(void* token, Args... args) noexcept
            {
                try
                {
                    const Ref<CoreClrHostContextState> state = FindContext(token);
                    if (state == nullptr)
                        return CW_MANAGED_STATUS_NOT_INITIALIZED;
                    // Only release may run on a finalizer thread. It queues work without touching scene or asset objects.
                    constexpr bool isFinalizerRelease = [] {
                        if constexpr (std::is_same_v<decltype(Member), decltype(&cw_managed_host_api::asset_release)>)
                            return Member == &cw_managed_host_api::asset_release;
                        return false;
                    }();
                    if (!isFinalizerRelease && state->OwnerThread != std::this_thread::get_id())
                        return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                    const auto invoke = [&]() -> cw_managed_status {
                        if (!state->Active || state->Bindings.*Member == nullptr)
                            return CW_MANAGED_STATUS_NOT_INITIALIZED;
                        return (state->Bindings.*Member)(token, args...);
                    };
                    if constexpr (isFinalizerRelease)
                    {
                        std::lock_guard lock(state->ReleaseMutex);
                        return invoke();
                    }
                    else
                    {
                        std::lock_guard lock(state->Mutex);
                        return invoke();
                    }
                }
                catch (...)
                {
                    return CW_MANAGED_STATUS_MANAGED_EXCEPTION;
                }
            }
        };

        String Decode(cw_managed_string_view value)
        {
            return value.data != nullptr ? String(reinterpret_cast<const char*>(value.data), value.length) : String();
        }

        void CW_MANAGED_CALL HostLog(void* token, uint32_t severity, cw_managed_string_view code, cw_managed_string_view message,
                                     cw_managed_string_view stack) noexcept
        {
            try
            {
                const Ref<CoreClrHostContextState> state = FindContext(token);
                if (state == nullptr)
                    return;
                // Managed workers may log while the owner waits for them inside a host callback.
                std::lock_guard lock(state->DiagnosticMutex);
                if (!state->Active)
                    return;
                ManagedDiagnostic diagnostic;
                diagnostic.Severity = severity == 0   ? ManagedDiagnosticSeverity::Info
                                      : severity == 1 ? ManagedDiagnosticSeverity::Warning
                                                      : ManagedDiagnosticSeverity::Error;
                diagnostic.Code = Decode(code);
                diagnostic.Message = Decode(message);
                diagnostic.ManagedStack = Decode(stack);
                diagnostic.Backend = ManagedBackendId::CoreCLR;
                state->Diagnostics.push_back(std::move(diagnostic));
            }
            catch (...)
            {
                // No exceptions may cross the unmanaged callback boundary.
            }
        }
    } // namespace

    CoreClrHostContext::CoreClrHostContext() : CoreClrHostContext(NativeApi()) {}

    CoreClrHostContext::CoreClrHostContext(const cw_managed_host_api& bindings) : m_State(CreateRef<CoreClrHostContextState>())
    {
        m_State->Bindings = bindings;
        m_Api.size = sizeof(m_Api);
        m_Api.abi_version = CW_MANAGED_ABI_VERSION;
        m_Api.log = &HostLog;
#define CW_GUARD_HOST_FUNCTION(functionName, fieldName)                                                                                              \
    m_Api.fieldName = &GuardedHostFunction<&cw_managed_host_api::fieldName, decltype(cw_managed_host_api::fieldName)>::Invoke;
        CW_MANAGED_HOST_FUNCTION_LIST(CW_GUARD_HOST_FUNCTION)
#undef CW_GUARD_HOST_FUNCTION
        HostContextRegistry& registry = Registry();
        Lock lock(registry.Guard);
        if (registry.NextToken == 0)
            throw std::overflow_error("CoreCLR host context tokens are exhausted.");
        m_Api.context = reinterpret_cast<void*>(registry.NextToken++);
        registry.Contexts.emplace(m_Api.context, m_State);
    }

    CoreClrHostContext::~CoreClrHostContext() { Revoke(); }

    void CoreClrHostContext::Revoke()
    {
        // A shared state keeps in-flight callbacks safe. A recursive mutex permits scene callbacks to reenter
        // the host or synchronously revoke it; no registry lock is held while calling engine code.
        std::lock_guard stateLock(m_State->Mutex);
        if (!m_State->Active)
            return;
        m_State->Active = false;
        HostContextRegistry& registry = Registry();
        {
            Lock registryLock(registry.Guard);
            registry.Contexts.erase(m_Api.context);
        }
        {
            std::lock_guard releaseLock(m_State->ReleaseMutex);
            ReleaseManagedHostBindings(m_Api.context);
        }
        {
            std::lock_guard diagnosticLock(m_State->DiagnosticMutex);
            m_State->Diagnostics.clear();
        }
    }

    Vector<ManagedDiagnostic> CoreClrHostContext::TakeDiagnostics()
    {
        if (m_State->OwnerThread != std::this_thread::get_id())
            return {};
        std::lock_guard lock(m_State->DiagnosticMutex);
        Vector<ManagedDiagnostic> result;
        if (!m_State->Active)
            return result;
        result.swap(m_State->Diagnostics);
        return result;
    }
} // namespace Crowny

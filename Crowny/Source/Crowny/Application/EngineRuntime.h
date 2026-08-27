#pragma once

#include "Crowny/Common/StdHeaders.h"

namespace Crowny
{

    struct ApplicationDesc;
    class ManagedScripting;

    /** Owns the engine services used by one Application instance.
     *
     * Modules remain globally reachable during the migration to dependency
     * injection, but this object is their single lifetime owner.
     */
    class EngineRuntime
    {
    public:
        explicit EngineRuntime(const ApplicationDesc& applicationDesc);
        ~EngineRuntime();

        EngineRuntime(const EngineRuntime&) = delete;
        EngineRuntime& operator=(const EngineRuntime&) = delete;

        void Start();
        void StartRenderer();
        void StartRuntimeServices();

        void StopRenderer();
        void ShutdownRendererResources();
        void ShutdownServices();
        void ShutdownCoreServices();
        void ShutdownRenderAPI();

        ManagedScripting* GetManagedScripting();
        const ManagedScripting* GetManagedScripting() const;

    private:
        struct State;
        Scope<State> m_State;
    };
} // namespace Crowny

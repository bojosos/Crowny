#pragma once

#include "Crowny/Common/StdHeaders.h"

namespace Crowny
{
    class Scene;

    class ScriptRuntime
    {
    public:
        static void Init();
        static void OnStart();
        static void OnStart(const Ref<Scene>& scene);
        static void OnUpdate();
        static void OnUpdate(const Ref<Scene>& scene);
        static void OnShutdown();
        static void OnShutdown(const Ref<Scene>& scene);

        static void Reload();
        static void UnloadAssemblies();
    };
} // namespace Crowny

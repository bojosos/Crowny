#pragma once

#include "Crowny/Common/StdHeaders.h"
#include "Crowny/Scripting/Managed/ManagedTypes.h"

namespace Crowny
{
    class Entity;
    class MonoScript;
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

        static bool CreateScript(Entity entity, MonoScript& script, bool dispatchStart);
        static void DestroyScript(Entity entity, MonoScript& script, bool dispatchDestroy = true);
        static void Dispatch(MonoScript& script, const ScriptEvent& event);
        static ScriptState CaptureState(MonoScript& script);
        static bool ApplyState(MonoScript& script, const ScriptState& state);

        static void Reload();
        static void UnloadAssemblies();
    };
} // namespace Crowny

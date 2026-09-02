#pragma once

#include "Crowny/Common/StdHeaders.h"
#include "Crowny/Common/Timestep.h"
#include "Crowny/Scripting/Managed/ManagedTypes.h"

namespace Crowny
{
    class Entity;
    class ManagedScript;
    class Scene;

    class ScriptRuntime
    {
    public:
        static void Init();
        static void OnStart();
        static void OnStart(const Ref<Scene>& scene);
        // Dispatches Update to every script, then LateUpdate to every script unless includeLateUpdate is false.
        // Frame loops that update animation between the two phases call OnUpdate(step, false) + OnLateUpdate(step).
        static void OnUpdate(Timestep timestep, bool includeLateUpdate = true);
        static void OnUpdate(const Ref<Scene>& scene, Timestep timestep, bool includeLateUpdate = true);
        static void OnLateUpdate(Timestep timestep);
        static void OnLateUpdate(const Ref<Scene>& scene, Timestep timestep);
        static void OnFixedUpdate(Timestep timestep);
        static void OnFixedUpdate(const Ref<Scene>& scene, Timestep timestep);
        static void OnShutdown();
        static void OnShutdown(const Ref<Scene>& scene);

        // dispatchStart awakens the script (Awake, then Start) right after construction.
        static bool CreateScript(Entity entity, ManagedScript& script, bool dispatchStart);
        // Awake + Start for a constructed script that has not been awakened yet. No-op otherwise.
        static void StartScript(Entity entity, ManagedScript& script);
        // True once Awake was dispatched. OnDestroy is only delivered to awakened scripts.
        static bool IsScriptAwake(const ManagedScript& script);
        // Dispatches OnDestroy (awakened scripts only, unless dispatchDestroy is false) and releases the instance.
        static void DestroyScript(Entity entity, ManagedScript& script, bool dispatchDestroy = true);
        static void Dispatch(ManagedScript& script, const ScriptEvent& event);
        static ScriptState CaptureState(ManagedScript& script);
        static bool ApplyState(ManagedScript& script, const ScriptState& state);
        static bool RunsInEditor(const ScriptTypeIdentity& identity);
        static void NotifyEntityDestroyed(const Entity& entity);
        static void NotifyComponentDestroyed(uint64_t instanceId);
        static void NotifySceneEventsAvailable();

        static void Reload();
        static void UnloadAssemblies();
    };
} // namespace Crowny

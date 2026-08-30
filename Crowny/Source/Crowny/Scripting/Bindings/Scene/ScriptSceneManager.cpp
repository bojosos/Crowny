#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptSceneManager.h"

#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoMethod.h"

namespace Crowny
{
    MonoMethod* ScriptSceneManager::s_NotifySceneEventMethod = nullptr;

    ScriptSceneManager::ScriptSceneManager() : ScriptObject() {}

    void ScriptSceneManager::InitRuntimeData() { s_NotifySceneEventMethod = MetaData.ScriptClass->GetMethod("NotifySceneEvent", 3); }

    void ScriptSceneManager::DispatchPendingEvents()
    {
        if (SceneManager::TryGet() == nullptr || !MonoManager::IsStartedUp() || s_NotifySceneEventMethod == nullptr)
            return;

        const Vector<SceneLifecycleEvent> events = SceneManager::TryGet()->DrainManagedLifecycleEvents();
        SceneManager::CallbackScope callbackScope = SceneManager::TryGet()->DeferSceneChanges();
        for (const SceneLifecycleEvent& event : events)
        {
            uint8_t type = static_cast<uint8_t>(event.Type);
            UUID sceneId = event.SceneId;
            uint8_t state = static_cast<uint8_t>(event.State);
            void* parameters[] = { &type, &sceneId, &state };
            s_NotifySceneEventMethod->Invoke(nullptr, parameters);
        }
    }
} // namespace Crowny

#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptSceneManager.h"

#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoMethod.h"

namespace Crowny
{
    MonoMethod* ScriptSceneManager::s_NotifySceneEventMethod = nullptr;

    ScriptSceneManager::ScriptSceneManager() : ScriptObject() {}

    void ScriptSceneManager::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetActiveScene", (void*)&Internal_GetActiveScene);
        MetaData.ScriptClass->AddInternalCall("Internal_GetExecutionState", (void*)&Internal_GetExecutionState);
        MetaData.ScriptClass->AddInternalCall("Internal_GetLoadedSceneCount", (void*)&Internal_GetLoadedSceneCount);
        MetaData.ScriptClass->AddInternalCall("Internal_GetLoadedScene", (void*)&Internal_GetLoadedScene);
        MetaData.ScriptClass->AddInternalCall("Internal_Load", (void*)&Internal_Load);
        MetaData.ScriptClass->AddInternalCall("Internal_Unload", (void*)&Internal_Unload);
        MetaData.ScriptClass->AddInternalCall("Internal_Reload", (void*)&Internal_Reload);
        MetaData.ScriptClass->AddInternalCall("Internal_SetActive", (void*)&Internal_SetActive);
        s_NotifySceneEventMethod = MetaData.ScriptClass->GetMethod("Internal_NotifySceneEvent", 3);
    }

    void ScriptSceneManager::Internal_GetActiveScene(UUID* sceneId)
    {
        if (sceneId != nullptr)
            *sceneId = SceneManager::TryGet() != nullptr ? SceneManager::TryGet()->GetActiveSceneId() : UUID::EMPTY;
    }

    uint8_t ScriptSceneManager::Internal_GetExecutionState()
    {
        return static_cast<uint8_t>(SceneManager::TryGet() != nullptr ? SceneManager::TryGet()->GetExecutionState() : SceneExecutionState::Edit);
    }

    uint32_t ScriptSceneManager::Internal_GetLoadedSceneCount()
    {
        return SceneManager::TryGet() != nullptr ? static_cast<uint32_t>(SceneManager::TryGet()->GetLoadedScenes().size()) : 0;
    }

    bool ScriptSceneManager::Internal_GetLoadedScene(uint32_t index, UUID* sceneId)
    {
        if (SceneManager::TryGet() == nullptr || sceneId == nullptr)
            return false;
        const Vector<UUID> scenes = SceneManager::TryGet()->GetLoadedScenes();
        if (index >= scenes.size())
            return false;
        *sceneId = scenes[index];
        return true;
    }

    uint8_t ScriptSceneManager::Internal_Load(UUID* sceneId, bool makeActive)
    {
        if (SceneManager::TryGet() == nullptr || sceneId == nullptr)
            return static_cast<uint8_t>(SceneOperationStatus::Failed);
        return static_cast<uint8_t>(SceneManager::TryGet()->LoadScene(*sceneId, makeActive));
    }

    uint8_t ScriptSceneManager::Internal_Unload(UUID* sceneId)
    {
        if (SceneManager::TryGet() == nullptr || sceneId == nullptr)
            return static_cast<uint8_t>(SceneOperationStatus::Failed);
        return static_cast<uint8_t>(SceneManager::TryGet()->UnloadScene(*sceneId));
    }

    uint8_t ScriptSceneManager::Internal_Reload(UUID* sceneId)
    {
        if (SceneManager::TryGet() == nullptr || sceneId == nullptr)
            return static_cast<uint8_t>(SceneOperationStatus::Failed);
        return static_cast<uint8_t>(SceneManager::TryGet()->ReloadScene(*sceneId));
    }

    uint8_t ScriptSceneManager::Internal_SetActive(UUID* sceneId)
    {
        if (SceneManager::TryGet() == nullptr || sceneId == nullptr)
            return static_cast<uint8_t>(SceneOperationStatus::Failed);
        return static_cast<uint8_t>(SceneManager::TryGet()->SetActiveScene(*sceneId));
    }

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

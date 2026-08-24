#pragma once

#include "Crowny/Common/Uuid.h"
#include "Crowny/Scripting/ScriptObject.h"

namespace Crowny
{
    class MonoMethod;

    class ScriptSceneManager : public ScriptObject<ScriptSceneManager>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "SceneManager")

        ScriptSceneManager();

        static void DispatchPendingEvents();

    private:
        static void Internal_GetActiveScene(UUID* sceneId);
        static uint8_t Internal_GetExecutionState();
        static uint32_t Internal_GetLoadedSceneCount();
        static bool Internal_GetLoadedScene(uint32_t index, UUID* sceneId);
        static uint8_t Internal_Load(UUID* sceneId, bool makeActive);
        static uint8_t Internal_Unload(UUID* sceneId);
        static uint8_t Internal_Reload(UUID* sceneId);
        static uint8_t Internal_SetActive(UUID* sceneId);

        static MonoMethod* s_NotifySceneEventMethod;
    };
} // namespace Crowny

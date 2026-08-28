#pragma once

#include "Crowny/Input/GamepadCodes.h"
#include "Crowny/Scripting/ScriptObject.h"

namespace Crowny
{
    class ScriptInput : public ScriptObject<ScriptInput>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Input")
        ScriptInput();

    private:
        static void Internal_GetMousePosition(glm::vec2* out);
        static void Internal_GetMouseDelta(glm::vec2* out);
        static bool Internal_IsGamepadConnected(uint32_t gamepad);
        static bool Internal_GetGamepadButton(GamepadButtonCode code, uint32_t gamepad);
        static bool Internal_GetGamepadButtonDown(GamepadButtonCode code, uint32_t gamepad);
        static bool Internal_GetGamepadButtonUp(GamepadButtonCode code, uint32_t gamepad);
        static float Internal_GetGamepadAxis(GamepadAxisCode code, uint32_t gamepad);
        static bool Internal_GetAction(MonoString* actionName);
        static bool Internal_GetActionDown(MonoString* actionName);
        static bool Internal_GetActionUp(MonoString* actionName);
        static float Internal_GetAxis(MonoString* actionName);
        static void Internal_GetActionVector(MonoString* actionName, glm::vec2* out);
        static bool Internal_EnableActionMap(MonoString* mapName);
        static bool Internal_DisableActionMap(MonoString* mapName);
        static void Internal_ClearActionRebinds();
    };
} // namespace Crowny

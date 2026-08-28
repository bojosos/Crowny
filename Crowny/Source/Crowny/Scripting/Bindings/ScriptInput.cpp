#include "cwpch.h"

#include "Crowny/Scripting/Bindings/ScriptInput.h"

#include "Crowny/Input/Input.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"

namespace Crowny
{
    ScriptInput::ScriptInput() : ScriptObject() {}

    void ScriptInput::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("GetKey", (void*)&Input::IsKeyPressed);
        MetaData.ScriptClass->AddInternalCall("GetKeyDown", (void*)&Input::IsKeyDown);
        MetaData.ScriptClass->AddInternalCall("GetKeyUp", (void*)&Input::IsKeyUp);
        MetaData.ScriptClass->AddInternalCall("GetMouseButton", (void*)&Input::IsMouseButtonPressed);
        MetaData.ScriptClass->AddInternalCall("GetMouseButtonDown", (void*)&Input::IsMouseButtonDown);
        MetaData.ScriptClass->AddInternalCall("GetMouseButtonUp", (void*)&Input::IsMouseButtonUp);
        MetaData.ScriptClass->AddInternalCall("GetMouseScrollX", (void*)&Input::GetMouseScrollX);
        MetaData.ScriptClass->AddInternalCall("GetMouseScrollY", (void*)&Input::GetMouseScrollY);
        MetaData.ScriptClass->AddInternalCall("Internal_GetMousePosition", (void*)&Internal_GetMousePosition);
        MetaData.ScriptClass->AddInternalCall("Internal_GetMouseDelta", (void*)&Internal_GetMouseDelta);
        MetaData.ScriptClass->AddInternalCall("IsGamepadConnected", (void*)&Internal_IsGamepadConnected);
        MetaData.ScriptClass->AddInternalCall("GetGamepadButton", (void*)&Internal_GetGamepadButton);
        MetaData.ScriptClass->AddInternalCall("GetGamepadButtonDown", (void*)&Internal_GetGamepadButtonDown);
        MetaData.ScriptClass->AddInternalCall("GetGamepadButtonUp", (void*)&Internal_GetGamepadButtonUp);
        MetaData.ScriptClass->AddInternalCall("GetGamepadAxis", (void*)&Internal_GetGamepadAxis);
        MetaData.ScriptClass->AddInternalCall("GetAction", (void*)&Internal_GetAction);
        MetaData.ScriptClass->AddInternalCall("GetActionDown", (void*)&Internal_GetActionDown);
        MetaData.ScriptClass->AddInternalCall("GetActionUp", (void*)&Internal_GetActionUp);
        MetaData.ScriptClass->AddInternalCall("GetAxis", (void*)&Internal_GetAxis);
        MetaData.ScriptClass->AddInternalCall("Internal_GetActionVector", (void*)&Internal_GetActionVector);
        MetaData.ScriptClass->AddInternalCall("EnableActionMap", (void*)&Internal_EnableActionMap);
        MetaData.ScriptClass->AddInternalCall("DisableActionMap", (void*)&Internal_DisableActionMap);
        MetaData.ScriptClass->AddInternalCall("ClearActionRebinds", (void*)&Internal_ClearActionRebinds);
    }

    void ScriptInput::Internal_GetMousePosition(glm::vec2* out) { *out = Input::GetMousePosition(); }

    void ScriptInput::Internal_GetMouseDelta(glm::vec2* out) { *out = Input::GetMouseDelta(); }

    bool ScriptInput::Internal_IsGamepadConnected(uint32_t gamepad) { return Input::IsGamepadConnected(gamepad); }

    bool ScriptInput::Internal_GetGamepadButton(GamepadButtonCode code, uint32_t gamepad) { return Input::IsGamepadButtonPressed(gamepad, code); }

    bool ScriptInput::Internal_GetGamepadButtonDown(GamepadButtonCode code, uint32_t gamepad) { return Input::IsGamepadButtonDown(gamepad, code); }

    bool ScriptInput::Internal_GetGamepadButtonUp(GamepadButtonCode code, uint32_t gamepad) { return Input::IsGamepadButtonUp(gamepad, code); }

    float ScriptInput::Internal_GetGamepadAxis(GamepadAxisCode code, uint32_t gamepad) { return Input::GetGamepadAxis(gamepad, code); }

    bool ScriptInput::Internal_GetAction(MonoString* actionName)
    {
        return actionName != nullptr && Input::GetAction(MonoUtils::FromMonoString(actionName));
    }

    bool ScriptInput::Internal_GetActionDown(MonoString* actionName)
    {
        return actionName != nullptr && Input::GetActionDown(MonoUtils::FromMonoString(actionName));
    }

    bool ScriptInput::Internal_GetActionUp(MonoString* actionName)
    {
        return actionName != nullptr && Input::GetActionUp(MonoUtils::FromMonoString(actionName));
    }

    float ScriptInput::Internal_GetAxis(MonoString* actionName)
    {
        return actionName != nullptr ? Input::GetAxis(MonoUtils::FromMonoString(actionName)) : 0.0f;
    }

    void ScriptInput::Internal_GetActionVector(MonoString* actionName, glm::vec2* out)
    {
        *out = actionName != nullptr ? Input::GetActionVector(MonoUtils::FromMonoString(actionName)) : glm::vec2(0.0f);
    }

    bool ScriptInput::Internal_EnableActionMap(MonoString* mapName)
    {
        return mapName != nullptr && Input::SetActionMapEnabled(MonoUtils::FromMonoString(mapName), true);
    }

    bool ScriptInput::Internal_DisableActionMap(MonoString* mapName)
    {
        return mapName != nullptr && Input::SetActionMapEnabled(MonoUtils::FromMonoString(mapName), false);
    }

    void ScriptInput::Internal_ClearActionRebinds() { Input::ClearActionRebinds(); }
} // namespace Crowny

#include "cwpch.h"

#include "Crowny/Scripting/Bindings/ScriptInput.h"

#include "Crowny/Input/Input.h"

namespace Crowny
{
    ScriptInput::ScriptInput() : ScriptObject() {}

    void ScriptInput::InitRuntimeData()
    {
        // // TODO: check for focus for the editor
        MetaData.ScriptClass->AddInternalCall("GetKey", (void*)&Input::IsKeyPressed);
        MetaData.ScriptClass->AddInternalCall("GetKeyDown", (void*)&Input::IsKeyDown);
        MetaData.ScriptClass->AddInternalCall("GetKeyUp", (void*)&Input::IsKeyUp);
        MetaData.ScriptClass->AddInternalCall("GetMouseButton", (void*)&Input::IsMouseButtonPressed);
        MetaData.ScriptClass->AddInternalCall("GetMouseButtonDown", (void*)&Input::IsMouseButtonDown);
        MetaData.ScriptClass->AddInternalCall("GetMouseButtonUp", (void*)&Input::IsMouseButtonUp);
        MetaData.ScriptClass->AddInternalCall("GetMouseScrollX", (void*)&Input::GetMouseScrollX);
        MetaData.ScriptClass->AddInternalCall("GetMouseScrollY", (void*)&Input::GetMouseScrollY);
    }
} // namespace Crowny

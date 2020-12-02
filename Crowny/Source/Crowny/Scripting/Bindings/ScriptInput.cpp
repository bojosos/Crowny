#include "cwpch.h"

#include "Crowny/Scripting/Bindings/ScriptInput.h"
#include "Crowny/Scripting/CWMonoRuntime.h"

#include "Crowny/Input/Input.h"

namespace Crowny
{
    void ScriptInput::InitRuntimeFunctions()
    {
        CWMonoClass* input = CWMonoRuntime::GetCrownyAssembly()->GetClass("Crowny", "Input");

        input->AddInternalCall("GetKey", (void*)&Input::IsKeyPressed);
        input->AddInternalCall("GetKeyDown", (void*)&Input::IsKeyDown);
        input->AddInternalCall("GetKeyUp", (void*)&Input::IsKeyUp);
        input->AddInternalCall("GetMouseButton", (void*)&Input::IsMouseButtonPressed);
        input->AddInternalCall("GetMouseButtonDown", (void*)&Input::IsMouseButtonDown);
        input->AddInternalCall("GetMouseButtonUp", (void*)&Input::IsMouseButtonUp);
    }
}
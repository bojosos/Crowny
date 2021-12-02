#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptCamera.h"

namespace Crowny
{

    void ScriptCamera::InitRuntimeData()
    {
        // MonoClass* cameraClass = MonoRuntime::GetCrownyAssembly()->GetClass("Crowny", "CameraComponent");

        // cameraClass->AddInternalCall("get_camera", (void*)&Internal_GetCamera);
        // cameraClass->AddInternalCall("set_camera", (void*)&Internal_SetCamera);
    }

    ScriptCamera::ScriptCamera(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

} // namespace Crowny
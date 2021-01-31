#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptCameraComponent.h"

#include "Crowny/Scripting/CWMonoRuntime.h"

namespace Crowny
{

    void ScriptCameraComponent::InitRuntimeFunctions()
    {
        CWMonoClass* cameraClass = CWMonoRuntime::GetCrownyAssembly()->GetClass("Crowny", "CameraComponent");
        
        cameraClass->AddInternalCall("get_camera", (void*)&Internal_GetCamera);
        cameraClass->AddInternalCall("set_camera", (void*)&Internal_SetCamera);
    }
    
    MonoObject* ScriptCameraComponent::Internal_GetCamera(CameraComponent* component)
    {
        
    }
        
    void ScriptCameraComponent::Internal_SetCamera(CameraComponent* component, Camera* camera)
    {
        
    }
    
}
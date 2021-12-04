#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptCamera.h"

namespace Crowny
{

    void ScriptCamera::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetCameraFov", (void*)&Internal_GetCameraFov);
        MetaData.ScriptClass->AddInternalCall("Internal_SetCameraFov", (void*)&Internal_SetCameraFov);
        MetaData.ScriptClass->AddInternalCall("Internal_GetCameraNearPlane", (void*)&Internal_GetCameraNearPlane);
        MetaData.ScriptClass->AddInternalCall("Internal_SetCameraNearPlane", (void*)&Internal_SetCameraNearPlane);
        MetaData.ScriptClass->AddInternalCall("Internal_GetCameraFarPlane", (void*)&Internal_GetCameraFarPlane);
        MetaData.ScriptClass->AddInternalCall("Internal_SetCameraFarPlane", (void*)&Internal_SetCameraFarPlane);
        MetaData.ScriptClass->AddInternalCall("Internal_GetCameraOrthographicSize", (void*)&Internal_GetCameraOrthographicSize);
        MetaData.ScriptClass->AddInternalCall("Internal_SetCameraOrthographicSize", (void*)&Internal_SetCameraOrthographicSize);
        MetaData.ScriptClass->AddInternalCall("Internal_GetCameraAspectRatio", (void*)&Internal_GetCameraAspectRatio);
        MetaData.ScriptClass->AddInternalCall("Internal_SetCameraAspectRatio", (void*)&Internal_SetCameraAspectRatio);
        MetaData.ScriptClass->AddInternalCall("Internal_GetCameraBackgroundColor", (void*)&Internal_GetCameraBackgroundColor);
        MetaData.ScriptClass->AddInternalCall("Internal_SetCameraBackgroundColor", (void*)&Internal_SetCameraBackgroundColor);
        MetaData.ScriptClass->AddInternalCall("Internal_GetCameraViewportRectangle", (void*)&Internal_GetCameraViewportRectangle);
        MetaData.ScriptClass->AddInternalCall("Internal_SetCameraViewportRectangle", (void*)&Internal_SetCameraViewportRectangle);
    }

    ScriptCamera::ScriptCamera(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    float ScriptCamera::Internal_GetCameraFov(ScriptCamera* thisPtr)
    {
        return thisPtr->GetComponent().Camera.GetPerspectiveVerticalFOV();
    }

    void ScriptCamera::Internal_SetCameraFov(ScriptCamera* thisPtr, float value)
    {
        thisPtr->GetComponent().Camera.SetPerspectiveVerticalFOV(value);
    }

    float ScriptCamera::Internal_GetCameraNearPlane(ScriptCamera* thisPtr)
    {
        CameraComponent& component = thisPtr->GetComponent();
        if (component.Camera.GetProjectionType() == SceneCamera::CameraProjection::Perspective)
            return component.Camera.GetPerspectiveNearClip();
        else
            return component.Camera.GetOrthographicNearClip();
    }

    void ScriptCamera::Internal_SetCameraNearPlane(ScriptCamera* thisPtr, float value)
    {
        CameraComponent& component = thisPtr->GetComponent();
        if (component.Camera.GetProjectionType() == SceneCamera::CameraProjection::Perspective)
            component.Camera.SetPerspectiveNearClip(value);
        else
            component.Camera.SetOrthographicNearClip(value);
    }

    float ScriptCamera::Internal_GetCameraFarPlane(ScriptCamera* thisPtr)
    {
        CameraComponent& component = thisPtr->GetComponent();
        if (component.Camera.GetProjectionType() == SceneCamera::CameraProjection::Perspective)
            return component.Camera.GetPerspectiveFarClip();
        else
            return component.Camera.GetOrthographicFarClip();
    }

    void ScriptCamera::Internal_SetCameraFarPlane(ScriptCamera* thisPtr, float value)
    {
        CameraComponent& component = thisPtr->GetComponent();
        if (component.Camera.GetProjectionType() == SceneCamera::CameraProjection::Perspective)
            component.Camera.SetPerspectiveFarClip(value);
        else
            component.Camera.SetOrthographicFarClip(value);
    }

    float ScriptCamera::Internal_GetCameraOrthographicSize(ScriptCamera* thisPtr)
    {
        CameraComponent& component = thisPtr->GetComponent();
        return component.Camera.GetOrthographicSize();
    }

    void ScriptCamera::Internal_SetCameraOrthographicSize(ScriptCamera* thisPtr, float value)
    {
        CameraComponent& component = thisPtr->GetComponent();
        component.Camera.SetOrthographicSize(value);
    }

    float ScriptCamera::Internal_GetCameraAspectRatio(ScriptCamera* thisPtr)
    {
        CameraComponent& component = thisPtr->GetComponent();
        return component.Camera.GetAspectRatio();
    }

    void ScriptCamera::Internal_SetCameraAspectRatio(ScriptCamera* thisPtr, float value)
    {
        CameraComponent& component = thisPtr->GetComponent();
        component.Camera.SetAspectRatio(value);
    }

    void ScriptCamera::Internal_GetCameraBackgroundColor(ScriptCamera* thisPtr, glm::vec3* value)
    {
        CameraComponent& component = thisPtr->GetComponent();
        *value = component.Camera.GetBackgroundColor();
    }

    void ScriptCamera::Internal_SetCameraBackgroundColor(ScriptCamera* thisPtr, glm::vec3* value)
    {
        CameraComponent& component = thisPtr->GetComponent();
        component.Camera.SetBackgroundColor(*value);
    }

    void ScriptCamera::Internal_GetCameraViewportRectangle(ScriptCamera* thisPtr, glm::vec4* value)
    {
        CameraComponent& component = thisPtr->GetComponent();
        *value = component.Camera.GetViewportRect();
    }

    void ScriptCamera::Internal_SetCameraViewportRectangle(ScriptCamera* thisPtr, glm::vec4* value)
    {
        CameraComponent& component = thisPtr->GetComponent();
        component.Camera.SetViewportRect(*value);
    }

} // namespace Crowny
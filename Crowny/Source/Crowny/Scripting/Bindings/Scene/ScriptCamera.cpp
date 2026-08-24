#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptCamera.h"

namespace Crowny
{

    void ScriptCamera::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetCameraFov", (void*)&Internal_GetCameraFov);
        MetaData.ScriptClass->AddInternalCall("Internal_SetCameraFov", (void*)&Internal_SetCameraFov);
        MetaData.ScriptClass->AddInternalCall("Internal_GetCameraProjection", (void*)&Internal_GetCameraProjection);
        MetaData.ScriptClass->AddInternalCall("Internal_SetCameraProjection", (void*)&Internal_SetCameraProjection);
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
        MetaData.ScriptClass->AddInternalCall("Internal_GetCameraHDR", (void*)&Internal_GetCameraHDR);
        MetaData.ScriptClass->AddInternalCall("Internal_SetCameraHDR", (void*)&Internal_SetCameraHDR);
        MetaData.ScriptClass->AddInternalCall("Internal_GetCameraMSAA", (void*)&Internal_GetCameraMSAA);
        MetaData.ScriptClass->AddInternalCall("Internal_SetCameraMSAA", (void*)&Internal_SetCameraMSAA);
        MetaData.ScriptClass->AddInternalCall("Internal_GetCameraOcclusionCulling", (void*)&Internal_GetCameraOcclusionCulling);
        MetaData.ScriptClass->AddInternalCall("Internal_SetCameraOcclusionCulling", (void*)&Internal_SetCameraOcclusionCulling);
    }

    ScriptCamera::ScriptCamera(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    float ScriptCamera::Internal_GetCameraFov(ScriptCamera* thisPtr)
    {
        return glm::degrees(thisPtr->GetComponent().Camera.GetPerspectiveVerticalFOV());
    }

    void ScriptCamera::Internal_SetCameraFov(ScriptCamera* thisPtr, float value)
    {
        thisPtr->GetComponent().Camera.SetPerspectiveVerticalFOV(glm::radians(value));
    }

    int32_t ScriptCamera::Internal_GetCameraProjection(ScriptCamera* thisPtr)
    {
        return static_cast<int32_t>(thisPtr->GetComponent().Camera.GetProjectionType());
    }

    void ScriptCamera::Internal_SetCameraProjection(ScriptCamera* thisPtr, int32_t value)
    {
        const SceneCamera::CameraProjection projection = value == static_cast<int32_t>(SceneCamera::CameraProjection::Perspective)
                                                           ? SceneCamera::CameraProjection::Perspective
                                                           : SceneCamera::CameraProjection::Orthographic;
        thisPtr->GetComponent().Camera.SetProjectionType(projection);
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

    bool ScriptCamera::Internal_GetCameraHDR(ScriptCamera* thisPtr) { return thisPtr->GetComponent().Camera.GetHDR(); }

    void ScriptCamera::Internal_SetCameraHDR(ScriptCamera* thisPtr, bool value) { thisPtr->GetComponent().Camera.SetHDR(value); }

    bool ScriptCamera::Internal_GetCameraMSAA(ScriptCamera* thisPtr) { return thisPtr->GetComponent().Camera.GetMSAA(); }

    void ScriptCamera::Internal_SetCameraMSAA(ScriptCamera* thisPtr, bool value) { thisPtr->GetComponent().Camera.SetMSAA(value); }

    bool ScriptCamera::Internal_GetCameraOcclusionCulling(ScriptCamera* thisPtr)
    {
        return thisPtr->GetComponent().Camera.GetOcclusionCulling();
    }

    void ScriptCamera::Internal_SetCameraOcclusionCulling(ScriptCamera* thisPtr, bool value)
    {
        thisPtr->GetComponent().Camera.SetOcclusionCulling(value);
    }

} // namespace Crowny

#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptTransform.h"

namespace Crowny
{

    ScriptTransform::ScriptTransform(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptTransform::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetPosition", (void*)&Internal_PositionGet);
        MetaData.ScriptClass->AddInternalCall("Internal_SetPosition", (void*)&Internal_PositionSet);
        MetaData.ScriptClass->AddInternalCall("Internal_GetLocalPosition", (void*)&Internal_LocalPositionGet);
        MetaData.ScriptClass->AddInternalCall("Internal_SetLocalPosition", (void*)&Internal_LocalPositionSet);

        MetaData.ScriptClass->AddInternalCall("Internal_GetRotation", (void*)&Internal_RotationGet);
        MetaData.ScriptClass->AddInternalCall("Internal_SetRotation", (void*)&Internal_RotationSet);
        MetaData.ScriptClass->AddInternalCall("Internal_GetLocalRotation", (void*)&Internal_LocalRotationGet);
        MetaData.ScriptClass->AddInternalCall("Internal_SetLocalRotation", (void*)&Internal_LocalRotationSet);
        MetaData.ScriptClass->AddInternalCall("Internal_GetEulerAngles", (void*)&Internal_EulerRotationGet);
        MetaData.ScriptClass->AddInternalCall("Internal_SetEulerAngles", (void*)&Internal_EulerRotationSet);
        MetaData.ScriptClass->AddInternalCall("Internal_GetLocalEulerAngles", (void*)&Internal_LocalEulerRotationGet);
        MetaData.ScriptClass->AddInternalCall("Internal_SetLocalEulerAngles", (void*)&Internal_LocalEulerRotationSet);

        MetaData.ScriptClass->AddInternalCall("Internal_GetScale", (void*)&Internal_ScaleGet);
        MetaData.ScriptClass->AddInternalCall("Internal_SetScale", (void*)&Internal_ScaleSet);
        MetaData.ScriptClass->AddInternalCall("Internal_GetLocalScale", (void*)&Internal_LocalScaleGet);
        MetaData.ScriptClass->AddInternalCall("Internal_SetLocalScale", (void*)&Internal_LocalScaleSet);

        MetaData.ScriptClass->AddInternalCall("Internal_GetWorldToLocalTransform",
                                              (void*)&Internal_GetWorldToLocalTransform);
        MetaData.ScriptClass->AddInternalCall("Internal_GetLocalToWorldMatrix", (void*)&Internal_GetLocalToWorldMatrix);
    }

    void ScriptTransform::Internal_PositionGet(ScriptTransform* thisPtr, glm::vec3* value)
    {
        Entity entity = thisPtr->GetNativeEntity();
        *value = entity.GetWorldPosition();
    }

    void ScriptTransform::Internal_PositionSet(ScriptTransform* thisPtr, glm::vec3* value)
    {
        Entity entity = thisPtr->GetNativeEntity();
        entity.SetWorldPosition(*value);
    }

    void ScriptTransform::Internal_LocalPositionGet(ScriptTransform* thisPtr, glm::vec3* value)
    {
        Entity entity = thisPtr->GetNativeEntity();
        *value = entity.GetLocalPosition();
    }

    void ScriptTransform::Internal_LocalPositionSet(ScriptTransform* thisPtr, glm::vec3* value) {}

    void ScriptTransform::Internal_EulerRotationGet(ScriptTransform* thisPtr, glm::vec3* value) {}

    void ScriptTransform::Internal_EulerRotationSet(ScriptTransform* thisPtr, glm::vec3* value) {}

    void ScriptTransform::Internal_LocalEulerRotationGet(ScriptTransform* thisPtr, glm::vec3* value)
    {
        Entity entity = thisPtr->GetNativeEntity();
        // *value = entity.GetLocalRotation();
    }

    void ScriptTransform::Internal_LocalEulerRotationSet(ScriptTransform* thisPtr, glm::vec3* value)
    {
        Entity entity = thisPtr->GetNativeEntity();
        // entity.SetRotation(*value);
    }

    void ScriptTransform::Internal_LocalRotationGet(ScriptTransform* thisPtr, glm::quat* value)
    {
        Entity entity = thisPtr->GetNativeEntity();
        *value = entity.GetLocalRotation();
    }

    void ScriptTransform::Internal_LocalRotationSet(ScriptTransform* thisPtr, glm::quat* value)
    {
        Entity entity = thisPtr->GetNativeEntity();
        entity.SetRotation(*value);
    }

    void ScriptTransform::Internal_RotationGet(ScriptTransform* thisPtr, glm::quat* value)
    {
        Entity entity = thisPtr->GetNativeEntity();
        *value = entity.GetWorldRotation();
    }

    void ScriptTransform::Internal_RotationSet(ScriptTransform* thisPtr, glm::quat* value)
    {
        Entity entity = thisPtr->GetNativeEntity();
        entity.SetWorldRotation(*value);
    }

    void ScriptTransform::Internal_ScaleGet(ScriptTransform* thisPtr, glm::vec3* value)
    {
        Entity entity = thisPtr->GetNativeEntity();
        *value = entity.GetWorldScale();
    }

    void ScriptTransform::Internal_ScaleSet(ScriptTransform* thisPtr, glm::vec3* value)
    {
        Entity entity = thisPtr->GetNativeEntity();
        entity.SetWorldScale(*value);
    }

    void ScriptTransform::Internal_LocalScaleGet(ScriptTransform* thisPtr, glm::vec3* value)
    {
        Entity entity = thisPtr->GetNativeEntity();
        *value = entity.GetLocalScale();
    }

    void ScriptTransform::Internal_LocalScaleSet(ScriptTransform* thisPtr, glm::vec3* value)
    {
        Entity entity = thisPtr->GetNativeEntity();
        entity.SetScale(*value);
    }

    void ScriptTransform::Internal_GetLocalToWorldMatrix(ScriptTransform* thisPtr, glm::mat4* value)
    {
        Entity entity = thisPtr->GetNativeEntity();
        *value = entity.GetWorldMatrix();
    }

    void ScriptTransform::Internal_GetWorldToLocalTransform(ScriptTransform* thisPtr, glm::mat4* value)
    {
        Entity entity = thisPtr->GetNativeEntity();
        *value = glm::inverse(entity.GetWorldMatrix());
    }

} // namespace Crowny

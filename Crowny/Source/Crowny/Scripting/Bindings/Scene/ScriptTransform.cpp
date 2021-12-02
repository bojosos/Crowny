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
        MetaData.ScriptClass->AddInternalCall("Internal_GetEulerAngles", (void*)&Internal_EulerRotationGet);
        MetaData.ScriptClass->AddInternalCall("Internal_SetEulerAngles", (void*)&Internal_EulerRotationSet);
        MetaData.ScriptClass->AddInternalCall("Internal_GetLocalEulerAngles", (void*)&Internal_LocalEulerRotationGet);
        MetaData.ScriptClass->AddInternalCall("Internal_SetLocalEulerAngles", (void*)&Internal_LocalEulerRotationSet);
        MetaData.ScriptClass->AddInternalCall("Internal_GetLocalScale", (void*)&Internal_LocalScaleGet);
        MetaData.ScriptClass->AddInternalCall("Internal_SetLocalScale", (void*)&Internal_LocalScaleSet);
    }

    void ScriptTransform::Internal_PositionGet(ScriptTransform* thisPtr, glm::vec3* value)
    {
        *value = thisPtr->GetComponent().Position;
    }

    void ScriptTransform::Internal_PositionSet(ScriptTransform* thisPtr, glm::vec3* value)
    {
        thisPtr->GetComponent().Position = *value;
    }

    void ScriptTransform::Internal_LocalPositionGet(ScriptTransform* thisPtr, glm::vec3* value)
    {
        *value = thisPtr->GetComponent().Position;
    }

    void ScriptTransform::Internal_LocalPositionSet(ScriptTransform* thisPtr, glm::vec3* value) {}

    void ScriptTransform::Internal_EulerRotationGet(ScriptTransform* thisPtr, glm::vec3* value) {}

    void ScriptTransform::Internal_EulerRotationSet(ScriptTransform* thisPtr, glm::vec3* value) {}

    void ScriptTransform::Internal_LocalEulerRotationGet(ScriptTransform* thisPtr, glm::vec3* value)
    {
        *value = thisPtr->GetComponent().Rotation;
    }

    void ScriptTransform::Internal_LocalEulerRotationSet(ScriptTransform* thisPtr, glm::vec3* value)
    {
        thisPtr->GetComponent().Rotation = *value;
    }

    void ScriptTransform::Internal_LocalScaleGet(ScriptTransform* thisPtr, glm::vec3* value)
    {
        *value = thisPtr->GetComponent().Scale;
    }

    void ScriptTransform::Internal_LocalScaleSet(ScriptTransform* thisPtr, glm::vec3* value)
    {
        thisPtr->GetComponent().Scale = *value;
    }

} // namespace Crowny

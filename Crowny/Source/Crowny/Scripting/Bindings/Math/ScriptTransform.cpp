#include "cwpch.h"

#include "Crowny/Scripting/CWMonoRuntime.h"

#include "Crowny/Scripting/Bindings/Math/ScriptTransform.h"

namespace Crowny
{

    void ScriptTransform::InitRuntimeFunctions()
    {
        CWMonoClass* transformClass = CWMonoRuntime::GetCrownyAssembly()->GetClass("Crowny", "Transform");

        transformClass->AddInternalCall("Internal_GetPosition", (void*)&Internal_PositionGet);
        transformClass->AddInternalCall("Internal_SetPosition", (void*)&Internal_PositionSet);
        transformClass->AddInternalCall("Internal_GetLocalPosition", (void*)&Internal_LocalPositionGet);
        transformClass->AddInternalCall("Internal_SetLocalPosition", (void*)&Internal_LocalPositionSet);
        transformClass->AddInternalCall("Internal_GetEulerAngles", (void*)&Internal_EulerRotationGet);
        transformClass->AddInternalCall("Internal_SetEulerAngles", (void*)&Internal_EulerRotationSet);
        transformClass->AddInternalCall("Internal_GetLocalEulerAngles", (void*)&Internal_LocalEulerRotationGet);
        transformClass->AddInternalCall("Internal_SetLocalEulerAngles", (void*)&Internal_LocalEulerRotationSet);
        transformClass->AddInternalCall("Internal_GetLocalScale", (void*)&Internal_LocalScaleGet);
        transformClass->AddInternalCall("Internal_SetLocalScale", (void*)&Internal_LocalScaleSet);
    }

    void ScriptTransform::Internal_PositionGet(TransformComponent* thisptr, glm::vec3* value)
    {
        *value = thisptr->Position;
    }

    void ScriptTransform::Internal_PositionSet(TransformComponent* thisptr, glm::vec3* value)
    {
        thisptr->Position = *value;
    }

    void ScriptTransform::Internal_LocalPositionGet(TransformComponent* thisptr, glm::vec3* value)
    {
        *value = thisptr->Position;
    }

    void ScriptTransform::Internal_LocalPositionSet(TransformComponent* thisptr, glm::vec3* value) {}

    void ScriptTransform::Internal_EulerRotationGet(TransformComponent* thisptr, glm::vec3* value) {}

    void ScriptTransform::Internal_EulerRotationSet(TransformComponent* thisptr, glm::vec3* value) {}

    void ScriptTransform::Internal_LocalEulerRotationGet(TransformComponent* thisptr, glm::vec3* value) {}

    void ScriptTransform::Internal_LocalEulerRotationSet(TransformComponent* thisptr, glm::vec3* value) {}

    void ScriptTransform::Internal_LocalScaleGet(TransformComponent* thisptr, glm::vec3* value) {}

    void ScriptTransform::Internal_LocalScaleSet(TransformComponent* thisptr, glm::vec3* value) {}

} // namespace Crowny

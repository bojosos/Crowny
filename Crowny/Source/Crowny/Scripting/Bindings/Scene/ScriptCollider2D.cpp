#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptCollider2D.h"

#include "Crowny/Scripting/ScriptSceneObjectManager.h"

#include <mono/metadata/object.h>

namespace Crowny
{

    ScriptCollider2DBase::ScriptCollider2DBase(MonoObject* instance) : ScriptComponentBase(instance) {}

    ScriptCollider2D::ScriptCollider2D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptCollider2D::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_IsTrigger", (void*)&Internal_IsTrigger);
        MetaData.ScriptClass->AddInternalCall("Internal_SetTrigger", (void*)&Internal_SetTrigger);
        MetaData.ScriptClass->AddInternalCall("Internal_GetOffset", (void*)&Internal_GetOffset);
        MetaData.ScriptClass->AddInternalCall("Internal_SetOffset", (void*)&Internal_SetOffset);
    }

    bool ScriptCollider2D::Internal_IsTrigger(ScriptCollider2D* thisPtr) { return thisPtr->GetComponent().IsTrigger; }

    void ScriptCollider2D::Internal_SetTrigger(ScriptCollider2D* thisPtr, bool trigger)
    {
        thisPtr->GetComponent().IsTrigger = trigger;
    }

    void ScriptCollider2D::Internal_GetOffset(ScriptCollider2D* thisPtr, glm::vec2* offset)
    {
        *offset = thisPtr->GetComponent().Offset;
    }

    void ScriptCollider2D::Internal_SetOffset(ScriptCollider2D* thisPtr, glm::vec2* offset)
    {
        thisPtr->GetComponent().Offset = *offset;
    }

    ScriptBoxCollider2D::ScriptBoxCollider2D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity)
    {
    }

    void ScriptBoxCollider2D::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetSize", (void*)&Internal_GetSize);
        MetaData.ScriptClass->AddInternalCall("Internal_SetSize", (void*)&Internal_SetSize);
    }

    void ScriptBoxCollider2D::Internal_GetSize(ScriptBoxCollider2D* thisPtr, glm::vec2* size)
    {
        *size = thisPtr->GetComponent().Size;
    }

    void ScriptBoxCollider2D::Internal_SetSize(ScriptBoxCollider2D* thisPtr, glm::vec2* size)
    {
        thisPtr->GetComponent().Size = *size;
    }

    ScriptCircleCollider2D::ScriptCircleCollider2D(MonoObject* instance, Entity entity)
      : TScriptComponent(instance, entity)
    {
    }

    void ScriptCircleCollider2D::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetRadius", (void*)&Internal_GetRadius);
        MetaData.ScriptClass->AddInternalCall("Internal_SetRadius", (void*)&Internal_SetRadius);
    }

    float ScriptCircleCollider2D::Internal_GetRadius(ScriptCircleCollider2D* thisPtr)
    {
        return thisPtr->GetComponent().Radius;
    }

    void ScriptCircleCollider2D::Internal_SetRadius(ScriptCircleCollider2D* thisPtr, float radius)
    {
        thisPtr->GetComponent().Radius = radius;
    }
} // namespace Crowny
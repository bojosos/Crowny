#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptCollider2D.h"

#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"

#include <mono/metadata/object.h>

namespace Crowny
{

    // --- Base class ---

    ScriptCollider2DBase::ScriptCollider2DBase(MonoObject* instance) : ScriptComponentBase(instance) {}

    bool ScriptCollider2DBase::Internal_IsTrigger(ScriptCollider2DBase* thisPtr) { return thisPtr->GetCollider2D().IsTrigger(); }

    void ScriptCollider2DBase::Internal_SetTrigger(ScriptCollider2DBase* thisPtr, bool trigger) { thisPtr->GetCollider2D().SetIsTrigger(trigger); }

    void ScriptCollider2DBase::Internal_GetOffset(ScriptCollider2DBase* thisPtr, glm::vec2* offset)
    {
        *offset = thisPtr->GetCollider2D().GetOffset();
    }

    void ScriptCollider2DBase::Internal_SetOffset(ScriptCollider2DBase* thisPtr, glm::vec2* offset)
    {
        // SetOffset is defined on the derived component types, not on Collider2D base.
        // We store the offset directly on the base.
        thisPtr->GetCollider2D().m_Offset = *offset;
    }

    // --- Collider2D (base wrapper) ---

    ScriptCollider2D::ScriptCollider2D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptCollider2D::InitRuntimeData()
    {
        // Register shared internal calls on the base Collider2D C# class.
        // Derived C# classes (BoxCollider2D, CircleCollider2D) inherit these.
        MetaData.ScriptClass->AddInternalCall("Internal_IsTrigger", (void*)&ScriptCollider2DBase::Internal_IsTrigger);
        MetaData.ScriptClass->AddInternalCall("Internal_SetTrigger", (void*)&ScriptCollider2DBase::Internal_SetTrigger);
        MetaData.ScriptClass->AddInternalCall("Internal_GetOffset", (void*)&ScriptCollider2DBase::Internal_GetOffset);
        MetaData.ScriptClass->AddInternalCall("Internal_SetOffset", (void*)&ScriptCollider2DBase::Internal_SetOffset);
    }

    // --- BoxCollider2D ---

    ScriptBoxCollider2D::ScriptBoxCollider2D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptBoxCollider2D::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetSize", (void*)&Internal_GetSize);
        MetaData.ScriptClass->AddInternalCall("Internal_SetSize", (void*)&Internal_SetSize);
    }

    void ScriptBoxCollider2D::Internal_GetSize(ScriptBoxCollider2D* thisPtr, glm::vec2* size) { *size = thisPtr->GetComponent().GetSize(); }

    void ScriptBoxCollider2D::Internal_SetSize(ScriptBoxCollider2D* thisPtr, glm::vec2* size)
    {
        thisPtr->GetComponent().SetSize(*size, thisPtr->GetNativeEntity());
    }

    // --- CircleCollider2D ---

    ScriptCircleCollider2D::ScriptCircleCollider2D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptCircleCollider2D::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetRadius", (void*)&Internal_GetRadius);
        MetaData.ScriptClass->AddInternalCall("Internal_SetRadius", (void*)&Internal_SetRadius);
    }

    float ScriptCircleCollider2D::Internal_GetRadius(ScriptCircleCollider2D* thisPtr) { return thisPtr->GetComponent().GetRadius(); }

    void ScriptCircleCollider2D::Internal_SetRadius(ScriptCircleCollider2D* thisPtr, float radius)
    {
        thisPtr->GetComponent().SetRadius(radius, thisPtr->GetNativeEntity());
    }

} // namespace Crowny

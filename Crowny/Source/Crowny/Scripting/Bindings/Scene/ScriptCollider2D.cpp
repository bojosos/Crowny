#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptCollider2D.h"

#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"

#include <mono/metadata/object.h>

namespace Crowny
{

    ScriptCollider2DBase::ScriptCollider2DBase(MonoObject* instance) : ScriptComponentBase(instance) {}

    ScriptCollider2D::ScriptCollider2D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity) {}

    void ScriptCollider2D::InitRuntimeData() { }

    ScriptBoxCollider2D::ScriptBoxCollider2D(MonoObject* instance, Entity entity) : TScriptComponent(instance, entity)
    {
    }

    void ScriptBoxCollider2D::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetSize", (void*)&Internal_GetSize);
		MetaData.ScriptClass->AddInternalCall("Internal_SetSize", (void*)&Internal_SetSize);

		MetaData.ScriptClass->AddInternalCall("Internal_IsTrigger", (void*)&Internal_IsTrigger);
		MetaData.ScriptClass->AddInternalCall("Internal_SetTrigger", (void*)&Internal_SetTrigger);
		MetaData.ScriptClass->AddInternalCall("Internal_GetOffset", (void*)&Internal_GetOffset);
		MetaData.ScriptClass->AddInternalCall("Internal_SetOffset", (void*)&Internal_SetOffset);
    }

    void ScriptBoxCollider2D::Internal_GetSize(ScriptBoxCollider2D* thisPtr, glm::vec2* size)
    {
        *size = thisPtr->GetComponent().GetSize();
    }

    void ScriptBoxCollider2D::Internal_SetSize(ScriptBoxCollider2D* thisPtr, glm::vec2* size)
    {
        thisPtr->GetComponent().SetSize(*size, thisPtr->GetNativeEntity());
    }

	bool ScriptBoxCollider2D::Internal_IsTrigger(ScriptBoxCollider2D* thisPtr) { return thisPtr->GetComponent().IsTrigger(); }

	void ScriptBoxCollider2D::Internal_SetTrigger(ScriptBoxCollider2D* thisPtr, bool trigger)
	{
		thisPtr->GetComponent().SetIsTrigger(trigger);
	}

	void ScriptBoxCollider2D::Internal_GetOffset(ScriptBoxCollider2D* thisPtr, glm::vec2* offset)
	{
		*offset = thisPtr->GetComponent().GetOffset();
	}

	void ScriptBoxCollider2D::Internal_SetOffset(ScriptBoxCollider2D* thisPtr, glm::vec2* offset)
	{
		thisPtr->GetComponent().SetOffset(*offset, thisPtr->GetNativeEntity());
	}

    ScriptCircleCollider2D::ScriptCircleCollider2D(MonoObject* instance, Entity entity)
      : TScriptComponent(instance, entity)
    {
    }

    void ScriptCircleCollider2D::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_GetRadius", (void*)&Internal_GetRadius);
        MetaData.ScriptClass->AddInternalCall("Internal_SetRadius", (void*)&Internal_SetRadius);

		MetaData.ScriptClass->AddInternalCall("Internal_IsTrigger", (void*)&Internal_IsTrigger);
		MetaData.ScriptClass->AddInternalCall("Internal_SetTrigger", (void*)&Internal_SetTrigger);
		MetaData.ScriptClass->AddInternalCall("Internal_GetOffset", (void*)&Internal_GetOffset);
		MetaData.ScriptClass->AddInternalCall("Internal_SetOffset", (void*)&Internal_SetOffset);
    }

    float ScriptCircleCollider2D::Internal_GetRadius(ScriptCircleCollider2D* thisPtr)
    {
        return thisPtr->GetComponent().GetRadius();
    }

    void ScriptCircleCollider2D::Internal_SetRadius(ScriptCircleCollider2D* thisPtr, float radius)
    {
        thisPtr->GetComponent().SetRadius(radius, thisPtr->GetNativeEntity());
    }
	
	bool ScriptCircleCollider2D::Internal_IsTrigger(ScriptCircleCollider2D* thisPtr) { return thisPtr->GetComponent().IsTrigger(); }

	void ScriptCircleCollider2D::Internal_SetTrigger(ScriptCircleCollider2D* thisPtr, bool trigger)
	{
		thisPtr->GetComponent().SetIsTrigger(trigger);
	}

	void ScriptCircleCollider2D::Internal_GetOffset(ScriptCircleCollider2D* thisPtr, glm::vec2* offset)
	{
		*offset = thisPtr->GetComponent().GetOffset();
	}

	void ScriptCircleCollider2D::Internal_SetOffset(ScriptCircleCollider2D* thisPtr, glm::vec2* offset)
	{
		thisPtr->GetComponent().SetOffset(*offset, thisPtr->GetNativeEntity());
	}
} // namespace Crowny
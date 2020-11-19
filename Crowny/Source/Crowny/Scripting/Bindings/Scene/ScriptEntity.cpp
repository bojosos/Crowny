#include "cwpch.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/SceneManagement/SceneManager.h"
#include "ScriptComponent.h"
#include "ScriptEntity.h"

namespace Crowny
{

	void ScriptEntity::InitRuntimeFunctions()
	{
		CWMonoClass* entityClass = CWMonoRuntime::GetCrownyAssembly()->GetClass("Crowny", "Entity");

		entityClass->AddInternalCall("Internal_GetName", (void*)&Internal_GetName);
		entityClass->AddInternalCall("Internal_SetName", (void*)&Internal_SetName);
		entityClass->AddInternalCall("Internal_GetParent", (void*)&Internal_GetParent);
		entityClass->AddInternalCall("Internal_GetTransform", (void*)&Internal_GetTransform);
	}

	MonoString* ScriptEntity::Internal_GetName(Entity* thisptr)
	{

	}
		
	void ScriptEntity::Internal_SetName(Entity* thisptr, MonoString* string)
	{

	}

	MonoObject* ScriptEntity::Internal_GetParent(Entity* thisptr)
	{

	}

	MonoObject* ScriptEntity::Internal_GetTransform(entt::entity e)
	{
		Entity entity = Entity(e, SceneManager::GetActiveScene());
		return entity.GetComponent<TransformComponent>().ManagedInstance;
	}

} 
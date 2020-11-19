#include "cwpch.h"

#include "ScriptComponent.h"
#include "Crowny/Scripting/CWMonoRuntime.h"

namespace Crowny
{
	std::unordered_map<entt::entity, MonoObject*> ScriptComponent::s_EntityComponents = {};

	void ScriptComponent::InitRuntimeFunctions()
	{
		CWMonoClass* componentClass = CWMonoRuntime::GetAssembly("")->GetClass("Crowny", "Component");

		componentClass->AddInternalCall("Internal_GetEntity", (void*)&Internal_GetEntity);
	}

	MonoObject* ScriptComponent::Internal_GetEntity(MonoScriptComponent* component)
	{
		return s_EntityComponents[component->ComponentParent.GetHandle()];
	}
}
#include "cwpch.h"

#include "ScriptComponent.h"
#include "Crowny/Scripting/CWMonoRuntime.h"

namespace Crowny
{
	std::unordered_map<uint32_t, MonoObject*> ScriptComponent::s_EntityComponents = {};

	void ScriptComponent::InitRuntimeFunctions()
	{
		CWMonoClass* componentClass = CWMonoRuntime::GetCrownyAssembly()->GetClass("Crowny", "Component");

		componentClass->AddInternalCall("Internal_GetEntity", (void*)&Internal_GetEntity);
	}

	MonoObject* ScriptComponent::Internal_GetEntity(MonoScriptComponent* component)
	{
		return s_EntityComponents[(uint32_t)component->ComponentParent.GetHandle()];
	}
}
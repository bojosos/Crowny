#pragma once

#include <entt/entt.hpp>

#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include <mono/metadata/object.h>

namespace Crowny
{

	class ScriptComponent
	{

	public:
		static void InitRuntimeFunctions();
		static std::unordered_map<uint32_t, MonoObject*> s_EntityComponents;
	private:
		static MonoObject* Internal_GetEntity(MonoScriptComponent* component);

		//static MonoObject* Internal_GetComponent(SceneObject* thisptr, MonoReflectionType type)
	};
}

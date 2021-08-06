#pragma once

#include "Crowny/Ecs/Components.h"
#include "ScriptComponent.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <mono/metadata/object.h>

namespace Crowny
{
    class ScriptEntity
    {

    public:
        static void InitRuntimeFunctions();
        // static unordered_map<Component, MonoObject *> s_ScriptComponents;

    private:
        static MonoString* Internal_GetName(Entity* thisptr);
        static void Internal_SetName(Entity* thisptr, MonoString* string);

        static MonoObject* Internal_GetParent(Entity* thisptr);
        static MonoObject* Internal_GetTransform(entt::entity thisptr);

        // static MonoObject* Internal_GetComponent(SceneObject* thisptr, MonoReflectionType type)
    };
} // namespace Crowny
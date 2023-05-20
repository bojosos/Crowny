#pragma once

#include "Crowny/Scripting/ScriptSceneObject.h"

namespace Crowny
{
    class ScriptEntity : public ScriptObject<ScriptEntity, ScriptSceneObjectBase>
    {

    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Entity")

        ScriptEntity(MonoObject* instance, Entity entity);

    private:
        static MonoString* Internal_GetName(ScriptEntity* thisPtr);
        static void Internal_SetName(ScriptEntity* thisPtr, MonoString* string);
        static MonoObject* Internal_GetParent(ScriptEntity* thisPtr);
        static void Internal_SetParent(ScriptEntity* thisPtr, MonoObject* parent);
        static void Internal_GetUUID(ScriptEntity* thisPtr, UUID42* uuid);
        static MonoObject* Internal_FindEntityByName(MonoString* name);

        static MonoObject* Internal_AddComponent(ScriptEntity* thisPtr, MonoReflectionType* type);
        static bool Internal_HasComponent(ScriptEntity* thisPtr, MonoReflectionType* type);
        static MonoObject* Internal_GetComponent(ScriptEntity* thisPtr, MonoReflectionType* type);
        static void Internal_RemoveComponent(ScriptEntity* thisPtr, MonoReflectionType* type);
    };
} // namespace Crowny
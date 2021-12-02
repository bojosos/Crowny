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
        static MonoString* Internal_GetName(ScriptEntity* thisptr);
        static void Internal_SetName(ScriptEntity* thisptr, MonoString* string);
        static MonoObject* Internal_GetParent(ScriptEntity* thisptr);
        static MonoObject* Internal_FindEntityByName(MonoString* name);
    };
} // namespace Crowny
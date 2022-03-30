#pragma once

#include "Crowny/Ecs/Components.h"
#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptCollider2DBase : public ScriptComponentBase
    {
    public:
        ScriptCollider2DBase(MonoObject* instance);
        virtual ~ScriptCollider2DBase() {}
    };

    class ScriptCollider2D : public TScriptComponent<ScriptCollider2D, Collider2D, ScriptCollider2DBase>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Collider2D");

        ScriptCollider2D(MonoObject* instance, Entity entity);

    private:
    };
} // namespace Crowny
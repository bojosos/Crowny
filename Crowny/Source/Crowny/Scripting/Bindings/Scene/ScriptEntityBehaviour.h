#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptEntityBehaviour : public TScriptComponent<ScriptEntityBehaviour, MonoScriptComponent>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "EntityBehaviour");

        ScriptEntityBehaviour(MonoObject* instance, Entity entity);
    };
} // namespace Crowny
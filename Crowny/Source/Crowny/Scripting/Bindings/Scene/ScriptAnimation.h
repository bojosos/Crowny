#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptAnimation : public TScriptComponent<ScriptAnimation, AnimationComponent>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "AnimationComponent");
        ScriptAnimation(MonoObject* instance, Entity entity);

    private:
    };
} // namespace Crowny

#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptSpriteAnimator : public TScriptComponent<ScriptSpriteAnimator, SpriteAnimatorComponent>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "SpriteAnimatorComponent");
        ScriptSpriteAnimator(MonoObject* instance, Entity entity);
    };
} // namespace Crowny

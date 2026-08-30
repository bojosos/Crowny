#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptSpriteRenderer : public TScriptComponent<ScriptSpriteRenderer, SpriteRendererComponent>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "SpriteRendererComponent")

        ScriptSpriteRenderer(MonoObject* instance, Entity entity);

    private:
    };
} // namespace Crowny

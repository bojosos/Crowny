#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptText : public TScriptComponent<ScriptText, TextComponent>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Text");

        ScriptText(MonoObject* instance, Entity entity);

    private:
    };
} // namespace Crowny

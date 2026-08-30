#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptLight : public TScriptComponent<ScriptLight, LightComponent>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "LightComponent");

        ScriptLight(MonoObject* instance, Entity entity);

    private:
    };
} // namespace Crowny

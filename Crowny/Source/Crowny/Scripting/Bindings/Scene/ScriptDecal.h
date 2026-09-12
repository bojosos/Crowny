#pragma once
#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptDecal : public TScriptComponent<ScriptDecal, DecalComponent>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "DecalComponent");
        ScriptDecal(MonoObject* instance, Entity entity);
    };
} // namespace Crowny

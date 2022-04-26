#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptMeshComponent : public TScriptComponent<ScriptMeshComponent, MeshRendererComponent>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "MeshComponent")

        ScriptMeshComponent(MonoObject* instance, Entity entity);
    };
} // namespace Crowny
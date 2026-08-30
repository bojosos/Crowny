#pragma once

#include "Crowny/Ecs/Components.h"
#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptRigidbody3D : public TScriptComponent<ScriptRigidbody3D, Rigidbody3DComponent>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Rigidbody3D")

        ScriptRigidbody3D(MonoObject* instance, Entity entity);

    private:
    };
} // namespace Crowny

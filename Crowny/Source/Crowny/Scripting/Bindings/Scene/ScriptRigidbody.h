#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptRigidbody2D : public TScriptComponent<ScriptRigidbody2D, Rigidbody2DComponent>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Rigidbody2D")

        ScriptRigidbody2D(MonoObject* instance, Entity entity);

    private:
    };

} // namespace Crowny

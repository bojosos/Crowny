#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

#include <glm/glm.hpp>

namespace Crowny
{

    class ScriptTransform : public TScriptComponent<ScriptTransform, TransformComponent>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Transform")

        ScriptTransform(MonoObject* instance, Entity entity);

    private:
    };
} // namespace Crowny

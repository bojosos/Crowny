#pragma once

#include "Crowny/Scripting/ScriptComponent.h"

namespace Crowny
{
    class ScriptCamera : public TScriptComponent<ScriptCamera, CameraComponent>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Camera");

        ScriptCamera(MonoObject* instance, Entity entity);
    };
} // namespace Crowny
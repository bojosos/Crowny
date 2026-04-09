#pragma once

#include "Crowny/Scripting/ScriptObject.h"

namespace Crowny
{
    class ScriptPhysics2D : public ScriptObject<ScriptPhysics2D>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Physics2D");

    private:
        static void Internal_Raycast(glm::vec2* origin, glm::vec2* direction, float distance, uint32_t layerMask, MonoArray** outResults);
    };
} // namespace Crowny

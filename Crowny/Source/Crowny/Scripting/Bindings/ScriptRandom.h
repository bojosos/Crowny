#pragma once

#include "Crowny/Scripting/ScriptObject.h"

#include <glm/glm.hpp>

namespace Crowny
{
    class ScriptRandom : public ScriptObject<ScriptRandom>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Random")

        ScriptRandom();

    private:
        static void Internal_InitState(int32_t seed);
        static void Internal_UnitCircle(glm::vec2* out);
        static void Internal_UnitSphere(glm::vec3* out);
        static float Internal_Range(float min, float max);
        static float Internal_Value();
    };
} // namespace Crowny
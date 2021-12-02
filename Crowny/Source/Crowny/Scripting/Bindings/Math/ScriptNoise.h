#pragma once

#include "Crowny/Scripting/ScriptObject.h"

namespace Crowny
{

    class ScriptNoise : public ScriptObject<ScriptNoise>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Noise")
        ScriptNoise();

    private:
        static float Internal_PerlinNoise(float x, float y);
    };
} // namespace Crowny

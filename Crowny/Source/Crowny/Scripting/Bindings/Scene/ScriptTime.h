#pragma once

#include "Crowny/Scripting/ScriptObject.h"

namespace Crowny
{
    class ScriptTime : public ScriptObject<ScriptTime>
    {

    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Time")
        ScriptTime();

    private:
        static float Internal_GetTime();
        static float Internal_GetDeltaTime();
        static float Internal_GetFixedDeltaTime();
        static float Internal_GetSmoothDeltaTime();
        static float Internal_RealtimeSinceStartup();
        static float Internal_GetFrameCount();
    };
} // namespace Crowny
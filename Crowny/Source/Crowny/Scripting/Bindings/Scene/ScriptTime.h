#pragma once

namespace Crowny
{
    class ScriptTime
    {

    public:
        static void InitRuntimeFunctions();

    private:
        static float Internal_GetTime();
        static float Internal_GetDeltaTime();
        static float Internal_GetFixedDeltaTime();
        static float Internal_GetSmoothDeltaTime();
        static float Internal_RealtimeSinceStartup();
        static float Internal_GetFrameCount();
    };
} // namespace Crowny
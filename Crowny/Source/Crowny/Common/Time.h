#pragma once

#include <cstdint>

namespace Crowny
{

    class Time
    {
    public:
        static float GetTime();
        static float GetDeltaTime();
        static uint64_t GetFrameCount();
        static float GetFixedDeltaTime();
        static float GetRealtimeSinceStartup();
        static float GetSmoothDeltaTime();

        // Internal — called by the engine loop, not user code
        static void Update(float deltaTime, float fixedDeltaTime);
        static void Reset();
    };

} // namespace Crowny

#pragma once

namespace Crowny
{

    class Time
    {
    public:
		static float GetTime();
		static float GetDeltaTime();
		static float GetFrameCount();
		static float GetFixedDeltaTime();
		static float GetRealtimeSinceStartup();
		static float GetSmoothDeltaTime();
    };
    
}
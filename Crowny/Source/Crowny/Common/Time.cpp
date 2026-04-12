#include "cwpch.h"

#include "Crowny/Common/Time.h"

namespace Crowny
{
    static float s_Time = 0.0f;
    static float s_DeltaTime = 0.0f;
    static float s_SmoothDeltaTime = 0.0f;
    static float s_FixedDeltaTime = 0.0f;
    static float s_RealtimeSinceStartup = 0.0f;
    static float s_FrameCount = 0.0f;

    void Time::Update(float deltaTime, float fixedDeltaTime)
    {
        s_FrameCount += 1.0f;
        s_DeltaTime = deltaTime;
        s_Time += deltaTime;
        s_RealtimeSinceStartup += deltaTime;
        s_SmoothDeltaTime = s_DeltaTime + s_Time / (s_FrameCount + 1.0f);
        s_FixedDeltaTime = fixedDeltaTime;
    }

    void Time::Reset()
    {
        s_Time = 0.0f;
        s_DeltaTime = 0.0f;
        s_SmoothDeltaTime = 0.0f;
        s_FixedDeltaTime = 0.0f;
        s_RealtimeSinceStartup = 0.0f;
        s_FrameCount = 0.0f;
    }

    float Time::GetTime() { return s_Time; }
    float Time::GetDeltaTime() { return s_DeltaTime; }
    float Time::GetFrameCount() { return s_FrameCount; }
    float Time::GetFixedDeltaTime() { return s_FixedDeltaTime; }
    float Time::GetRealtimeSinceStartup() { return s_RealtimeSinceStartup; }
    float Time::GetSmoothDeltaTime() { return s_SmoothDeltaTime; }
} // namespace Crowny

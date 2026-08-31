#pragma once

#include "Crowny/Common/RefCounted.h"
#include "Crowny/Common/Timestep.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Crowny
{
    class TimeSettingsSerializer;

    struct TimeSettings : public RefCounted
    {
        float TimeScale = 1.0f;
        float MaxTimestep = 1.0f / 3.0f;
        float FixedTimestep = 0.02f;

        using Serializer = TimeSettingsSerializer;
    };

    class Time
    {
    public:
        void BeginFrame(Timestep unscaledDeltaTime);
        void AdvanceSimulation(const TimeSettings& settings);
        void ResetSimulation();

        float GetTime() const { return static_cast<float>(m_Time); }
        float GetDeltaTime() const { return m_DeltaTime; }
        float GetUnscaledDeltaTime() const { return m_UnscaledDeltaTime; }
        uint64_t GetFrameCount() const { return m_FrameCount; }
        float GetFixedDeltaTime() const { return m_FixedDeltaTime; }
        float GetRealtimeSinceStartup() const { return static_cast<float>(m_RealtimeSinceStartup); }
        float GetSmoothDeltaTime() const { return m_SmoothDeltaTime; }

    private:
        void AddSmoothDeltaSample(float deltaTime);

        static constexpr size_t SMOOTH_DELTA_SAMPLE_COUNT = 60;

        double m_Time = 0.0;
        double m_RealtimeSinceStartup = 0.0;
        float m_DeltaTime = 0.0f;
        float m_UnscaledDeltaTime = 0.0f;
        float m_SmoothDeltaTime = 0.0f;
        float m_FixedDeltaTime = 0.0f;
        uint64_t m_FrameCount = 0;
        uint64_t m_LastSimulationFrame = 0;
        std::array<float, SMOOTH_DELTA_SAMPLE_COUNT> m_DeltaSamples{};
        size_t m_DeltaSampleIndex = 0;
        size_t m_DeltaSampleCount = 0;
        double m_DeltaSampleSum = 0.0;
    };
} // namespace Crowny

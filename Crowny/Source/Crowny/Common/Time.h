#pragma once

#include "Crowny/Common/RefCounted.h"
#include "Crowny/Common/Timestep.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace Crowny
{
    class TimeSettingsSerializer;

    struct TimeSettings : public RefCounted
    {
        float TimeScale = 1.0f;
        float MaxTimestep = 1.0f / 3.0f;
        float FixedTimestep = 0.02f;
        uint32_t MaxFixedStepsPerFrame = 17;

        using Serializer = TimeSettingsSerializer;
    };

    struct SimulationFrame
    {
        Timestep FrameDelta;
        Timestep FixedDelta;
        uint32_t FixedStepCount = 0;
        float FixedRemainder = 0.0f;
        float InterpolationAlpha = 0.0f;
        float DroppedTime = 0.0f;
    };

    class Time
    {
        class CallbackScope
        {
        public:
            CallbackScope(Time& time, float deltaTime);
            ~CallbackScope();

            CallbackScope(const CallbackScope&) = delete;
            CallbackScope& operator=(const CallbackScope&) = delete;

        private:
            Time& m_Time;
            float m_PreviousDeltaTime = 0.0f;
            uint32_t m_PreviousDepth = 0;
        };

    public:
        void BeginFrame(Timestep unscaledDeltaTime);
        // Scales and limits the latest application-frame delta once, then returns the complete simulation schedule for that frame.
        SimulationFrame AdvanceSimulation(const TimeSettings& settings);
        void ResetSimulation();

        // Runs zero or more fixed ticks, one variable update, and one render-preparation callback in that order.
        // GetDeltaTime reports FixedDelta while a fixed callback is executing.
        template <typename FixedUpdate, typename Update, typename PrepareRender>
        void ExecuteSimulationFrame(const SimulationFrame& frame, FixedUpdate&& fixedUpdate, Update&& update,
                                    PrepareRender&& prepareRender)
        {
            for (uint32_t step = 0; step < frame.FixedStepCount; ++step)
            {
                CallbackScope callbackScope(*this, frame.FixedDelta.GetSeconds());
                std::invoke(fixedUpdate, frame.FixedDelta);
            }
            std::invoke(update, frame.FrameDelta);
            std::invoke(prepareRender, frame.InterpolationAlpha, Timestep(frame.FixedRemainder));
        }

        float GetTime() const { return static_cast<float>(m_Time); }
        float GetDeltaTime() const { return m_CallbackDeltaDepth > 0 ? m_CallbackDeltaTime : m_DeltaTime; }
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
        double m_FixedAccumulator = 0.0;
        float m_DeltaTime = 0.0f;
        float m_UnscaledDeltaTime = 0.0f;
        float m_SmoothDeltaTime = 0.0f;
        float m_FixedDeltaTime = 0.0f;
        float m_CallbackDeltaTime = 0.0f;
        uint32_t m_CallbackDeltaDepth = 0;
        uint64_t m_FrameCount = 0;
        uint64_t m_LastSimulationFrame = 0;
        SimulationFrame m_LastSimulationFramePlan;
        std::array<float, SMOOTH_DELTA_SAMPLE_COUNT> m_DeltaSamples{};
        size_t m_DeltaSampleIndex = 0;
        size_t m_DeltaSampleCount = 0;
        double m_DeltaSampleSum = 0.0;
    };
} // namespace Crowny

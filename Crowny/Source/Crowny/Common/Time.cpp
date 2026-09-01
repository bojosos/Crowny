#include "cwpch.h"

#include "Crowny/Common/Time.h"

#include <algorithm>
#include <cmath>

namespace Crowny
{
    namespace
    {
        float NonNegativeFinite(float value, float fallback = 0.0f)
        {
            return std::isfinite(value) && value >= 0.0f ? value : fallback;
        }
    } // namespace

    Time::CallbackScope::CallbackScope(Time& time, float deltaTime)
        : m_Time(time), m_PreviousDeltaTime(time.m_CallbackDeltaTime), m_PreviousDepth(time.m_CallbackDeltaDepth)
    {
        m_Time.m_CallbackDeltaTime = NonNegativeFinite(deltaTime);
        ++m_Time.m_CallbackDeltaDepth;
    }

    Time::CallbackScope::~CallbackScope()
    {
        m_Time.m_CallbackDeltaTime = m_PreviousDeltaTime;
        m_Time.m_CallbackDeltaDepth = m_PreviousDepth;
    }

    void Time::BeginFrame(Timestep unscaledDeltaTime)
    {
        m_UnscaledDeltaTime = NonNegativeFinite(unscaledDeltaTime.GetSeconds());
        m_RealtimeSinceStartup += static_cast<double>(m_UnscaledDeltaTime);
        m_DeltaTime = 0.0f;
        ++m_FrameCount;
    }

    SimulationFrame Time::AdvanceSimulation(const TimeSettings& settings)
    {
        if (m_FrameCount == 0 || m_LastSimulationFrame == m_FrameCount)
        {
            SimulationFrame repeated = m_LastSimulationFramePlan;
            repeated.FrameDelta = Timestep(0.0f);
            repeated.FixedStepCount = 0;
            repeated.DroppedTime = 0.0f;
            return repeated;
        }

        const float fixedDeltaTime = std::max(NonNegativeFinite(settings.FixedTimestep, 0.02f), 0.0001f);
        const float maxTimestep = std::max(NonNegativeFinite(settings.MaxTimestep, 1.0f / 3.0f), fixedDeltaTime);
        const float timeScale = NonNegativeFinite(settings.TimeScale);
        const uint32_t maxFixedSteps = std::max(settings.MaxFixedStepsPerFrame, 1u);

        const double scaledDeltaTime = static_cast<double>(m_UnscaledDeltaTime) * static_cast<double>(timeScale);
        const double fixedStepBudget = static_cast<double>(fixedDeltaTime) * static_cast<double>(maxFixedSteps);
        const double acceptedDeltaTime = std::min({ scaledDeltaTime, static_cast<double>(maxTimestep), fixedStepBudget });

        m_DeltaTime = static_cast<float>(acceptedDeltaTime);
        m_FixedDeltaTime = fixedDeltaTime;
        m_Time += static_cast<double>(m_DeltaTime);
        m_FixedAccumulator += acceptedDeltaTime;
        m_LastSimulationFrame = m_FrameCount;
        AddSmoothDeltaSample(m_DeltaTime);

        const double fixedStep = static_cast<double>(fixedDeltaTime);
        const double comparisonTolerance = fixedStep * 1.0e-6;
        const uint32_t fixedStepCount = std::min(
          static_cast<uint32_t>(std::floor((m_FixedAccumulator + comparisonTolerance) / fixedStep)), maxFixedSteps);
        m_FixedAccumulator -= static_cast<double>(fixedStepCount) * fixedStep;
        if (m_FixedAccumulator < comparisonTolerance)
            m_FixedAccumulator = 0.0;

        m_LastSimulationFramePlan.FrameDelta = Timestep(m_DeltaTime);
        m_LastSimulationFramePlan.FixedDelta = Timestep(fixedDeltaTime);
        m_LastSimulationFramePlan.FixedStepCount = fixedStepCount;
        m_LastSimulationFramePlan.FixedRemainder = static_cast<float>(m_FixedAccumulator);
        m_LastSimulationFramePlan.InterpolationAlpha =
          std::clamp(static_cast<float>(m_FixedAccumulator / fixedStep), 0.0f, 1.0f);
        m_LastSimulationFramePlan.DroppedTime = static_cast<float>(std::max(scaledDeltaTime - acceptedDeltaTime, 0.0));
        return m_LastSimulationFramePlan;
    }

    void Time::ResetSimulation()
    {
        m_Time = 0.0;
        m_FixedAccumulator = 0.0;
        m_DeltaTime = 0.0f;
        m_SmoothDeltaTime = 0.0f;
        m_FixedDeltaTime = 0.0f;
        m_LastSimulationFrame = 0;
        m_LastSimulationFramePlan = {};
        m_DeltaSamples.fill(0.0f);
        m_DeltaSampleIndex = 0;
        m_DeltaSampleCount = 0;
        m_DeltaSampleSum = 0.0;
    }

    void Time::AddSmoothDeltaSample(float deltaTime)
    {
        if (m_DeltaSampleCount == m_DeltaSamples.size())
            m_DeltaSampleSum -= static_cast<double>(m_DeltaSamples[m_DeltaSampleIndex]);
        else
            ++m_DeltaSampleCount;

        m_DeltaSamples[m_DeltaSampleIndex] = deltaTime;
        m_DeltaSampleSum += static_cast<double>(deltaTime);
        m_DeltaSampleIndex = (m_DeltaSampleIndex + 1) % m_DeltaSamples.size();
        m_SmoothDeltaTime = static_cast<float>(m_DeltaSampleSum / static_cast<double>(m_DeltaSampleCount));
    }
} // namespace Crowny

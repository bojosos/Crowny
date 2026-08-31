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

    void Time::BeginFrame(Timestep unscaledDeltaTime)
    {
        m_UnscaledDeltaTime = NonNegativeFinite(unscaledDeltaTime.GetSeconds());
        m_RealtimeSinceStartup += static_cast<double>(m_UnscaledDeltaTime);
        m_DeltaTime = 0.0f;
        ++m_FrameCount;
    }

    void Time::AdvanceSimulation(const TimeSettings& settings)
    {
        if (m_FrameCount == 0 || m_LastSimulationFrame == m_FrameCount)
            return;

        const float fixedDeltaTime = std::max(NonNegativeFinite(settings.FixedTimestep, 0.02f), 0.0001f);
        const float maxTimestep = std::max(NonNegativeFinite(settings.MaxTimestep, 1.0f / 3.0f), fixedDeltaTime);
        const float timeScale = NonNegativeFinite(settings.TimeScale);

        m_DeltaTime = std::min(m_UnscaledDeltaTime, maxTimestep) * timeScale;
        m_FixedDeltaTime = fixedDeltaTime;
        m_Time += static_cast<double>(m_DeltaTime);
        m_LastSimulationFrame = m_FrameCount;
        AddSmoothDeltaSample(m_DeltaTime);
    }

    void Time::ResetSimulation()
    {
        m_Time = 0.0;
        m_DeltaTime = 0.0f;
        m_SmoothDeltaTime = 0.0f;
        m_FixedDeltaTime = 0.0f;
        m_LastSimulationFrame = 0;
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

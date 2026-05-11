#include "cwpch.h"

#include "AnimationCurve.h"

namespace Crowny
{
    template <typename T> AnimationCurve<T>::AnimationCurve(const Vector<KeyFrameType>& keyframes) : m_Keyframes(keyframes)
    {
        m_Start = keyframes.front().Time;
        m_End = keyframes.empty() ? 0.0f : keyframes.back().Time;
        m_Length = m_End - m_Start;
    }

    template <typename T> typename AnimationCurve<T>::InterpolationKeyFrames AnimationCurve<T>::FindKeyFrames(float time) const
    {
        int32_t left = 0;
        int32_t right = (int32_t)m_Keyframes.size();
        while (right > 0)
        {
            const int32_t half = right / 2;
            const int32_t mid = left + half;
            if (time < m_Keyframes[mid].Time)
                right = half;
            else
            {
                left = mid + 1;
                right -= half - 1;
            }
        }
        return { m_Keyframes[std::max(0, left - 1)], m_Keyframes[std::min(left, (int32_t)m_Keyframes.size() - 1)] };
    }

    template <typename T> T AnimationCurve<T>::Evaluate(float time, const bool loop) const
    {
        if (loop && m_Length)
        {
            if (time < m_Start)
                time = time + std::floor(m_End - time) * m_Length;
            else if (time > m_End)
                time = time - std::floor(time - m_Start) * m_Length;
        }
        const auto [left, right] = FindKeyFrames(time);
        const float k = (time - left.Time) / (right.Time - time);
        return (1.0f - k) * left.Value + k * right.Value;
    }

    template class AnimationCurve<float>;
    template class AnimationCurve<glm::vec3>;
} // namespace Crowny
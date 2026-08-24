#include "cwpch.h"

#include "Crowny/Animation/AnimationCurve.h"

#include <glm/gtx/quaternion.hpp>

namespace Crowny
{
    template <typename T> AnimationCurve<T>::AnimationCurve(Vector<KeyFrameType> keyframes) { SetKeyFrames(std::move(keyframes)); }

    template <typename T> void AnimationCurve<T>::SetKeyFrames(Vector<KeyFrameType> keyframes)
    {
        keyframes.erase(std::remove_if(keyframes.begin(), keyframes.end(), [](const KeyFrameType& key) { return !std::isfinite(key.Time); }),
                        keyframes.end());
        std::stable_sort(keyframes.begin(), keyframes.end(),
                         [](const KeyFrameType& left, const KeyFrameType& right) { return left.Time < right.Time; });

        m_Keyframes.clear();
        m_Keyframes.reserve(keyframes.size());
        for (KeyFrameType& key : keyframes)
        {
            if (!m_Keyframes.empty() && glm::abs(m_Keyframes.back().Time - key.Time) <= std::numeric_limits<float>::epsilon())
                m_Keyframes.back() = std::move(key);
            else
                m_Keyframes.push_back(std::move(key));
        }

        if (m_Keyframes.empty())
        {
            m_Start = 0.0f;
            m_End = 0.0f;
            m_Length = 0.0f;
            return;
        }

        m_Start = m_Keyframes.front().Time;
        m_End = m_Keyframes.back().Time;
        m_Length = std::max(0.0f, m_End - m_Start);
    }

    template <typename T> float AnimationCurve<T>::WrapTime(float time, AnimationWrapMode wrapMode) const
    {
        if (m_Length <= std::numeric_limits<float>::epsilon() || wrapMode == AnimationWrapMode::Clamp)
            return glm::clamp(time, m_Start, m_End);

        float offset = std::fmod(time - m_Start, m_Length);
        if (wrapMode == AnimationWrapMode::Loop)
        {
            if (offset < 0.0f)
                offset += m_Length;
            return m_Start + offset;
        }

        const float period = m_Length * 2.0f;
        offset = std::fmod(time - m_Start, period);
        if (offset < 0.0f)
            offset += period;
        if (offset > m_Length)
            offset = period - offset;
        return m_Start + offset;
    }

    template <typename T> uint32_t AnimationCurve<T>::FindLeftKey(float time, AnimationCurveCache* cache) const
    {
        if (m_Keyframes.size() < 2)
            return 0;

        if (cache != nullptr && cache->Key < m_Keyframes.size() - 1)
        {
            const uint32_t key = cache->Key;
            if (time >= m_Keyframes[key].Time && time <= m_Keyframes[key + 1].Time)
                return key;
        }

        const auto right =
          std::upper_bound(m_Keyframes.begin(), m_Keyframes.end(), time, [](float value, const KeyFrameType& key) { return value < key.Time; });
        const uint32_t left = right == m_Keyframes.begin() ? 0
                                                           : std::min(static_cast<uint32_t>(std::distance(m_Keyframes.begin(), right) - 1),
                                                                      static_cast<uint32_t>(m_Keyframes.size() - 2));
        if (cache != nullptr)
            cache->Key = left;
        return left;
    }

    template <typename T> typename AnimationCurve<T>::InterpolationKeyFrames AnimationCurve<T>::FindKeyFrames(float time) const
    {
        static const KeyFrameType emptyKey{};
        if (m_Keyframes.empty())
            return { emptyKey, emptyKey };
        if (m_Keyframes.size() == 1)
            return { m_Keyframes.front(), m_Keyframes.front() };

        const uint32_t left = FindLeftKey(glm::clamp(time, m_Start, m_End), nullptr);
        return { m_Keyframes[left], m_Keyframes[left + 1] };
    }

    template <typename T> T AnimationCurve<T>::Interpolate(const KeyFrameType& left, const KeyFrameType& right, float time)
    {
        const float duration = right.Time - left.Time;
        if (duration <= std::numeric_limits<float>::epsilon() || left.Interpolation == AnimationInterpolation::Step)
            return left.Value;

        const float t = glm::clamp((time - left.Time) / duration, 0.0f, 1.0f);
        if constexpr (std::is_same_v<T, glm::quat>)
        {
            return glm::normalize(glm::slerp(left.Value, right.Value, t));
        }
        else
        {
            if (left.Interpolation == AnimationInterpolation::Linear)
                return left.Value * (1.0f - t) + right.Value * t;

            const float t2 = t * t;
            const float t3 = t2 * t;
            return left.Value * (2.0f * t3 - 3.0f * t2 + 1.0f) + left.OutTangent * (duration * (t3 - 2.0f * t2 + t)) +
                   right.Value * (-2.0f * t3 + 3.0f * t2) + right.InTangent * (duration * (t3 - t2));
        }
    }

    template <typename T> T AnimationCurve<T>::Evaluate(float time, bool loop) const
    {
        return Evaluate(time, loop ? AnimationWrapMode::Loop : AnimationWrapMode::Clamp);
    }

    template <typename T> T AnimationCurve<T>::Evaluate(float time, AnimationWrapMode wrapMode) const
    {
        AnimationCurveCache cache;
        return Evaluate(time, cache, wrapMode);
    }

    template <typename T> T AnimationCurve<T>::Evaluate(float time, AnimationCurveCache& cache, AnimationWrapMode wrapMode) const
    {
        if (m_Keyframes.empty())
            return T{};
        if (m_Keyframes.size() == 1)
            return m_Keyframes.front().Value;

        const float wrappedTime = WrapTime(time, wrapMode);
        if (wrappedTime >= m_End)
            return m_Keyframes.back().Value;
        const uint32_t left = FindLeftKey(wrappedTime, &cache);
        return Interpolate(m_Keyframes[left], m_Keyframes[left + 1], wrappedTime);
    }

    template class AnimationCurve<float>;
    template class AnimationCurve<glm::vec2>;
    template class AnimationCurve<glm::vec3>;
    template class AnimationCurve<glm::vec4>;
    template class AnimationCurve<glm::quat>;
} // namespace Crowny

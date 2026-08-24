#pragma once

#include "Crowny/Common/Types.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Crowny
{
    enum class AnimationInterpolation : uint8_t
    {
        Step,
        Linear,
        Cubic
    };

    enum class AnimationWrapMode : uint8_t
    {
        Clamp,
        Loop,
        PingPong
    };

    template <typename T> struct KeyFrame
    {
        float Time = 0.0f;
        T Value{};
        T InTangent{};
        T OutTangent{};
        AnimationInterpolation Interpolation = AnimationInterpolation::Linear;
    };

    /** Cached segment used when a curve is sampled sequentially. One cache belongs to one curve. */
    struct AnimationCurveCache
    {
        uint32_t Key = std::numeric_limits<uint32_t>::max();

        void Reset() { Key = std::numeric_limits<uint32_t>::max(); }
    };

    template <typename T> class AnimationCurve
    {
    public:
        using KeyFrameType = KeyFrame<T>;
        using InterpolationKeyFrames = Tuple<const KeyFrameType&, const KeyFrameType&>;

        AnimationCurve() = default;
        explicit AnimationCurve(Vector<KeyFrameType> keyframes);

        void SetKeyFrames(Vector<KeyFrameType> keyframes);
        const Vector<KeyFrameType>& GetKeyFrames() const { return m_Keyframes; }

        bool IsEmpty() const { return m_Keyframes.empty(); }
        uint32_t GetKeyFrameCount() const { return static_cast<uint32_t>(m_Keyframes.size()); }
        float GetStartTime() const { return m_Start; }
        float GetEndTime() const { return m_End; }
        float GetLength() const { return m_Length; }

        InterpolationKeyFrames FindKeyFrames(float time) const;
        T Evaluate(float time, bool loop) const;
        T Evaluate(float time, AnimationWrapMode wrapMode = AnimationWrapMode::Clamp) const;
        T Evaluate(float time, AnimationCurveCache& cache, AnimationWrapMode wrapMode = AnimationWrapMode::Clamp) const;

    private:
        float WrapTime(float time, AnimationWrapMode wrapMode) const;
        uint32_t FindLeftKey(float time, AnimationCurveCache* cache) const;
        static T Interpolate(const KeyFrameType& left, const KeyFrameType& right, float time);

        float m_Start = 0.0f;
        float m_End = 0.0f;
        float m_Length = 0.0f;
        Vector<KeyFrameType> m_Keyframes;
    };
} // namespace Crowny

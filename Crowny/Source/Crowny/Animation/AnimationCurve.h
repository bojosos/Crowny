#pragma once

namespace Crowny
{
    // TODO: Do we need other specializations? Like for int, string etc...
    template <typename T> struct KeyFrame
    {
        // TODO: Tangents? assimp always linear...
        float Time;
        T Value;
    };

    template <typename T> class AnimationCurve
    {
    public:
        using KeyFrameType = KeyFrame<T>;
        using InterpolationKeyFrames = Tuple<const KeyFrameType&, const KeyFrameType&>;

        AnimationCurve() = default;
        AnimationCurve(const Vector<KeyFrameType>& keyframes);

        InterpolationKeyFrames FindKeyFrames(float time) const;
        T Evaluate(float time, bool loop) const;

    private:
        float m_Start = 0.0f, m_End = 0.0f, m_Length = 0.0f;
        Vector<KeyFrameType> m_Keyframes;
    };
} // namespace Crowny
#include "cwpch.h"

#include "Crowny/Animation/AnimationClip.h"

namespace Crowny
{
    namespace
    {
        template <typename T> float CurveEnd(const AnimationCurve<T>& curve) { return curve.IsEmpty() ? 0.0f : curve.GetEndTime(); }

        template <typename T, typename Operation> void TransformCurve(AnimationCurve<T>& curve, Operation&& operation)
        {
            Vector<KeyFrame<T>> keys = curve.GetKeyFrames();
            for (KeyFrame<T>& key : keys)
                operation(key.Value);
            curve.SetKeyFrames(std::move(keys));
        }
    } // namespace

    AnimationClip::AnimationClip(const AnimationCurve<float>& curve) : AnimationClip({}, {}, { AnimationGenericTrack{ "Value", curve } }) {}

    AnimationClip::AnimationClip(Vector<AnimationTransformTrack> transformTracks, Vector<AnimationMorphTrack> morphTracks,
                                 Vector<AnimationGenericTrack> genericTracks, RootMotionCurves rootMotion, float sampleRate, bool additive)
      : m_SampleRate(sampleRate), m_Additive(additive), m_RootMotion(std::move(rootMotion)), m_TransformTracks(std::move(transformTracks)),
        m_MorphTracks(std::move(morphTracks)), m_GenericTracks(std::move(genericTracks))
    {
        SetSampleRate(sampleRate);
        RebuildLookup();
        RecalculateLength();
    }

    void AnimationClip::SetSampleRate(float sampleRate) { m_SampleRate = std::isfinite(sampleRate) && sampleRate > 0.0f ? sampleRate : 30.0f; }

    void AnimationClip::SetTransformTracks(Vector<AnimationTransformTrack> tracks)
    {
        m_TransformTracks = std::move(tracks);
        RebuildLookup();
        RecalculateLength();
    }

    void AnimationClip::SetMorphTracks(Vector<AnimationMorphTrack> tracks)
    {
        m_MorphTracks = std::move(tracks);
        RebuildLookup();
        RecalculateLength();
    }

    void AnimationClip::SetGenericTracks(Vector<AnimationGenericTrack> tracks)
    {
        m_GenericTracks = std::move(tracks);
        RebuildLookup();
        RecalculateLength();
    }

    void AnimationClip::SetRootMotion(RootMotionCurves rootMotion)
    {
        m_RootMotion = std::move(rootMotion);
        RecalculateLength();
    }

    void AnimationClip::SetEvents(Vector<AnimationEvent> events)
    {
        events.erase(std::remove_if(events.begin(), events.end(), [](const AnimationEvent& event) { return !std::isfinite(event.Time); }),
                     events.end());
        std::stable_sort(events.begin(), events.end(),
                         [](const AnimationEvent& left, const AnimationEvent& right) { return left.Time < right.Time; });
        m_Events = std::move(events);
        RecalculateLength();
    }

    void AnimationClip::RebuildLookup()
    {
        m_TransformLookup.clear();
        m_MorphLookup.clear();
        m_GenericLookup.clear();
        for (uint32_t index = 0; index < m_TransformTracks.size(); index++)
            m_TransformLookup.insert_or_assign(m_TransformTracks[index].Name, index);
        for (uint32_t index = 0; index < m_MorphTracks.size(); index++)
            m_MorphLookup.insert_or_assign(m_MorphTracks[index].Name, index);
        for (uint32_t index = 0; index < m_GenericTracks.size(); index++)
            m_GenericLookup.insert_or_assign(m_GenericTracks[index].Name, index);
    }

    void AnimationClip::RecalculateLength()
    {
        m_Length = std::max(CurveEnd(m_RootMotion.Position), CurveEnd(m_RootMotion.Rotation));
        for (const AnimationTransformTrack& track : m_TransformTracks)
            m_Length = std::max({ m_Length, CurveEnd(track.Position), CurveEnd(track.Rotation), CurveEnd(track.Scale) });
        for (const AnimationMorphTrack& track : m_MorphTracks)
            m_Length = std::max(m_Length, CurveEnd(track.Weight));
        for (const AnimationGenericTrack& track : m_GenericTracks)
            m_Length = std::max(m_Length, CurveEnd(track.Curve));
        if (!m_Events.empty())
            m_Length = std::max(m_Length, m_Events.back().Time);
    }

    int32_t AnimationClip::FindTransformTrack(StringView name) const
    {
        const auto found = m_TransformLookup.find(name);
        return found == m_TransformLookup.end() ? -1 : static_cast<int32_t>(found->second);
    }

    int32_t AnimationClip::FindMorphTrack(StringView name) const
    {
        const auto found = m_MorphLookup.find(name);
        return found == m_MorphLookup.end() ? -1 : static_cast<int32_t>(found->second);
    }

    int32_t AnimationClip::FindGenericTrack(StringView name) const
    {
        const auto found = m_GenericLookup.find(name);
        return found == m_GenericLookup.end() ? -1 : static_cast<int32_t>(found->second);
    }

    Transform AnimationClip::SampleTransform(uint32_t trackIndex, float time, AnimationWrapMode wrapMode, const Transform& fallback) const
    {
        if (trackIndex >= m_TransformTracks.size())
            return fallback;

        const AnimationTransformTrack& track = m_TransformTracks[trackIndex];
        return Transform(track.Position.IsEmpty() ? fallback.GetPosition() : track.Position.Evaluate(time, wrapMode),
                         track.Rotation.IsEmpty() ? fallback.GetRotation() : track.Rotation.Evaluate(time, wrapMode),
                         track.Scale.IsEmpty() ? fallback.GetScale() : track.Scale.Evaluate(time, wrapMode));
    }

    float AnimationClip::SampleMorphWeight(uint32_t trackIndex, float time, AnimationWrapMode wrapMode) const
    {
        return trackIndex < m_MorphTracks.size() ? m_MorphTracks[trackIndex].Weight.Evaluate(time, wrapMode) : 0.0f;
    }

    float AnimationClip::SampleGeneric(uint32_t trackIndex, float time, AnimationWrapMode wrapMode) const
    {
        return trackIndex < m_GenericTracks.size() ? m_GenericTracks[trackIndex].Curve.Evaluate(time, wrapMode) : 0.0f;
    }

    void AnimationClip::MakeAdditive(float referenceTime)
    {
        for (AnimationTransformTrack& track : m_TransformTracks)
        {
            const glm::vec3 referencePosition = track.Position.IsEmpty() ? glm::vec3(0.0f) : track.Position.Evaluate(referenceTime);
            const glm::quat referenceRotation = track.Rotation.IsEmpty() ? glm::quat(1.0f, 0.0f, 0.0f, 0.0f) : track.Rotation.Evaluate(referenceTime);
            const glm::vec3 referenceScale = track.Scale.IsEmpty() ? glm::vec3(1.0f) : track.Scale.Evaluate(referenceTime);
            TransformCurve(track.Position, [&](glm::vec3& value) { value -= referencePosition; });
            TransformCurve(track.Rotation, [&](glm::quat& value) { value = glm::normalize(glm::inverse(referenceRotation) * value); });
            TransformCurve(track.Scale, [&](glm::vec3& value) {
                value = glm::vec3(referenceScale.x != 0.0f ? value.x / referenceScale.x : 1.0f,
                                  referenceScale.y != 0.0f ? value.y / referenceScale.y : 1.0f,
                                  referenceScale.z != 0.0f ? value.z / referenceScale.z : 1.0f);
            });
        }
        for (AnimationMorphTrack& track : m_MorphTracks)
        {
            const float reference = track.Weight.IsEmpty() ? 0.0f : track.Weight.Evaluate(referenceTime);
            TransformCurve(track.Weight, [&](float& value) { value -= reference; });
        }
        for (AnimationGenericTrack& track : m_GenericTracks)
        {
            const float reference = track.Curve.IsEmpty() ? 0.0f : track.Curve.Evaluate(referenceTime);
            TransformCurve(track.Curve, [&](float& value) { value -= reference; });
        }
        m_Additive = true;
    }

    Ref<AnimationClip> AnimationClip::Create(const AnimationCurve<float>& curve) { return CreateRef<AnimationClip>(curve); }

    Ref<AnimationClip> AnimationClip::Create(Vector<AnimationTransformTrack> transformTracks, Vector<AnimationMorphTrack> morphTracks,
                                             Vector<AnimationGenericTrack> genericTracks, RootMotionCurves rootMotion, float sampleRate,
                                             bool additive)
    {
        return CreateRef<AnimationClip>(std::move(transformTracks), std::move(morphTracks), std::move(genericTracks), std::move(rootMotion),
                                        sampleRate, additive);
    }
} // namespace Crowny

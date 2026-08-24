#pragma once

#include "Crowny/Animation/AnimationCurve.h"
#include "Crowny/Assets/Asset.h"
#include "Crowny/Common/HashedString.h"
#include "Crowny/Common/Math.h"

namespace Crowny
{
    struct AnimationTransformTrack
    {
        String Name;
        AnimationCurve<glm::vec3> Position;
        AnimationCurve<glm::quat> Rotation;
        AnimationCurve<glm::vec3> Scale;
    };

    struct AnimationMorphTrack
    {
        String Name;
        AnimationCurve<float> Weight;
    };

    struct AnimationGenericTrack
    {
        String Name;
        AnimationCurve<float> Curve;
    };

    struct RootMotionCurves
    {
        AnimationCurve<glm::vec3> Position;
        AnimationCurve<glm::quat> Rotation;

        bool IsEmpty() const { return Position.IsEmpty() && Rotation.IsEmpty(); }
    };

    struct AnimationEvent
    {
        String Name;
        float Time = 0.0f;
        String Payload;
    };

    class AnimationClip : public Asset
    {
    public:
        AnimationClip() = default;
        explicit AnimationClip(const AnimationCurve<float>& curve);
        AnimationClip(Vector<AnimationTransformTrack> transformTracks, Vector<AnimationMorphTrack> morphTracks = {},
                      Vector<AnimationGenericTrack> genericTracks = {}, RootMotionCurves rootMotion = {}, float sampleRate = 30.0f,
                      bool additive = false);
        virtual ~AnimationClip() override = default;

        AssetType GetAssetType() const override { return AssetType::AnimationClip; }
        static AssetType GetStaticType() { return AssetType::AnimationClip; }

        float GetLength() const { return m_Length; }
        float GetSampleRate() const { return m_SampleRate; }
        bool IsAdditive() const { return m_Additive; }
        const Vector<AnimationTransformTrack>& GetTransformTracks() const { return m_TransformTracks; }
        const Vector<AnimationMorphTrack>& GetMorphTracks() const { return m_MorphTracks; }
        const Vector<AnimationGenericTrack>& GetGenericTracks() const { return m_GenericTracks; }
        const RootMotionCurves& GetRootMotion() const { return m_RootMotion; }
        const Vector<AnimationEvent>& GetEvents() const { return m_Events; }

        void SetSampleRate(float sampleRate);
        void SetAdditive(bool additive) { m_Additive = additive; }
        void SetTransformTracks(Vector<AnimationTransformTrack> tracks);
        void SetMorphTracks(Vector<AnimationMorphTrack> tracks);
        void SetGenericTracks(Vector<AnimationGenericTrack> tracks);
        void SetRootMotion(RootMotionCurves rootMotion);
        void SetEvents(Vector<AnimationEvent> events);

        int32_t FindTransformTrack(StringView name) const;
        int32_t FindMorphTrack(StringView name) const;
        int32_t FindGenericTrack(StringView name) const;

        Transform SampleTransform(uint32_t trackIndex, float time, AnimationWrapMode wrapMode, const Transform& fallback = Transform()) const;
        float SampleMorphWeight(uint32_t trackIndex, float time, AnimationWrapMode wrapMode) const;
        float SampleGeneric(uint32_t trackIndex, float time, AnimationWrapMode wrapMode) const;

        /** Converts this clip to deltas from a reference time, suitable for additive layers. */
        void MakeAdditive(float referenceTime = 0.0f);

        static Ref<AnimationClip> Create(const AnimationCurve<float>& curve);
        static Ref<AnimationClip> Create(Vector<AnimationTransformTrack> transformTracks, Vector<AnimationMorphTrack> morphTracks = {},
                                         Vector<AnimationGenericTrack> genericTracks = {}, RootMotionCurves rootMotion = {}, float sampleRate = 30.0f,
                                         bool additive = false);

    private:
        void RebuildLookup();
        void RecalculateLength();
        CW_SERIALIZABLE(AnimationClip);

        float m_SampleRate = 30.0f;
        float m_Length = 0.0f;
        bool m_Additive = false;
        RootMotionCurves m_RootMotion;
        Vector<AnimationTransformTrack> m_TransformTracks;
        Vector<AnimationMorphTrack> m_MorphTracks;
        Vector<AnimationGenericTrack> m_GenericTracks;
        Vector<AnimationEvent> m_Events;
        UnorderedMap<String, uint32_t, StringHash, StringEqual> m_TransformLookup;
        UnorderedMap<String, uint32_t, StringHash, StringEqual> m_MorphLookup;
        UnorderedMap<String, uint32_t, StringHash, StringEqual> m_GenericLookup;
    };
} // namespace Crowny

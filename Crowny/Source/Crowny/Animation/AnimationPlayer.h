#pragma once

#include "Crowny/Animation/Skeleton.h"

namespace Crowny
{
    enum class AnimationPlaybackState : uint8_t
    {
        Stopped,
        Playing,
        Paused
    };

    /** Runtime clip playback, cross-fading, event dispatch, pose evaluation, and morph-weight evaluation. */
    class AnimationPlayer : public RefCounted
    {
    public:
        using EventCallback = std::function<void(const AnimationEvent&)>;

        void Play(const Ref<AnimationClip>& clip, bool restart = true);
        void CrossFade(const Ref<AnimationClip>& clip, float duration);
        void Stop();
        void Pause();
        void Resume();
        void Seek(float time);

        void SetSpeed(float speed) { m_Speed = std::isfinite(speed) ? speed : 1.0f; }
        float GetSpeed() const { return m_Speed; }
        void SetWrapMode(AnimationWrapMode wrapMode) { m_WrapMode = wrapMode; }
        AnimationWrapMode GetWrapMode() const { return m_WrapMode; }
        float GetTime() const { return m_Time; }
        AnimationPlaybackState GetState() const { return m_State; }
        const Ref<AnimationClip>& GetClip() const { return m_Clip; }
        void SetEventCallback(EventCallback callback) { m_EventCallback = std::move(callback); }

        void Update(float deltaTime, const Ref<Skeleton>& skeleton = nullptr, const Ref<MeshMorph>& morph = nullptr);

        const SkeletonPose& GetPose() const { return m_OutputPose; }
        const Vector<float>& GetMorphWeights() const { return m_MorphWeights; }
        const Transform& GetRootMotionDelta() const { return m_RootMotionDelta; }

    private:
        void Evaluate(const Ref<Skeleton>& skeleton, const Ref<MeshMorph>& morph);
        void EvaluateMorphWeights(const AnimationClip& clip, float time, const MeshMorph& morph, Vector<float>& weights) const;
        void DispatchEvents(const AnimationClip& clip, float previousTime, float currentTime) const;
        void CalculateRootMotion(const AnimationClip& clip, float previousTime, float currentTime);
        static float NormalizeTime(const AnimationClip& clip, float time, AnimationWrapMode wrapMode);

        Ref<AnimationClip> m_Clip;
        Ref<AnimationClip> m_FadeFromClip;
        float m_Time = 0.0f;
        float m_PreviousTime = 0.0f;
        float m_FadeFromTime = 0.0f;
        float m_FadeDuration = 0.0f;
        float m_FadeElapsed = 0.0f;
        float m_Speed = 1.0f;
        AnimationWrapMode m_WrapMode = AnimationWrapMode::Loop;
        AnimationPlaybackState m_State = AnimationPlaybackState::Stopped;
        SkeletonPose m_OutputPose;
        SkeletonPose m_CurrentPose;
        SkeletonPose m_FadePose;
        Vector<float> m_MorphWeights;
        Vector<float> m_CurrentMorphWeights;
        Vector<float> m_FadeMorphWeights;
        Transform m_RootMotionDelta;
        EventCallback m_EventCallback;
    };
} // namespace Crowny

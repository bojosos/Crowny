#include "cwpch.h"

#include "Crowny/Animation/AnimationPlayer.h"

namespace Crowny
{
    namespace
    {
        Transform IdentityTransform() { return Transform(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f)); }
    } // namespace

    void AnimationPlayer::Play(const Ref<AnimationClip>& clip, bool restart)
    {
        if (!clip)
        {
            Stop();
            return;
        }
        if (restart || clip != m_Clip)
            m_Time = 0.0f;
        m_PreviousTime = m_Time;
        m_Clip = clip;
        m_FadeFromClip = nullptr;
        m_FadeElapsed = 0.0f;
        m_FadeDuration = 0.0f;
        m_State = AnimationPlaybackState::Playing;
    }

    void AnimationPlayer::CrossFade(const Ref<AnimationClip>& clip, float duration)
    {
        if (!clip || !m_Clip || duration <= 0.0f)
        {
            Play(clip);
            return;
        }
        m_FadeFromClip = m_Clip;
        m_FadeFromTime = m_Time;
        m_Clip = clip;
        m_Time = 0.0f;
        m_PreviousTime = 0.0f;
        m_FadeDuration = duration;
        m_FadeElapsed = 0.0f;
        m_State = AnimationPlaybackState::Playing;
    }

    void AnimationPlayer::Stop()
    {
        m_State = AnimationPlaybackState::Stopped;
        m_Time = 0.0f;
        m_PreviousTime = 0.0f;
        m_FadeFromClip = nullptr;
        m_RootMotionDelta = IdentityTransform();
    }

    void AnimationPlayer::Pause()
    {
        if (m_State == AnimationPlaybackState::Playing)
            m_State = AnimationPlaybackState::Paused;
    }

    void AnimationPlayer::Resume()
    {
        if (m_State == AnimationPlaybackState::Paused && m_Clip)
            m_State = AnimationPlaybackState::Playing;
    }

    void AnimationPlayer::Seek(float time)
    {
        m_Time = std::isfinite(time) ? time : 0.0f;
        m_PreviousTime = m_Time;
    }

    float AnimationPlayer::NormalizeTime(const AnimationClip& clip, float time, AnimationWrapMode wrapMode)
    {
        const float length = clip.GetLength();
        if (length <= std::numeric_limits<float>::epsilon() || wrapMode == AnimationWrapMode::Clamp)
            return glm::clamp(time, 0.0f, length);
        if (wrapMode == AnimationWrapMode::Loop)
        {
            float result = std::fmod(time, length);
            return result < 0.0f ? result + length : result;
        }
        const float period = length * 2.0f;
        float result = std::fmod(time, period);
        if (result < 0.0f)
            result += period;
        return result > length ? period - result : result;
    }

    void AnimationPlayer::DispatchEvents(const AnimationClip& clip, float previousTime, float currentTime) const
    {
        if (!m_EventCallback || clip.GetEvents().empty() || previousTime == currentTime)
            return;

        const float length = clip.GetLength();
        const bool forward = currentTime > previousTime;
        const auto dispatchRange = [&](float start, float end, bool includeStart) {
            if (forward)
            {
                for (const AnimationEvent& event : clip.GetEvents())
                {
                    if ((includeStart ? event.Time >= start : event.Time > start) && event.Time <= end)
                        m_EventCallback(event);
                }
            }
            else
            {
                for (auto event = clip.GetEvents().rbegin(); event != clip.GetEvents().rend(); ++event)
                {
                    if ((includeStart ? event->Time <= start : event->Time < start) && event->Time >= end)
                        m_EventCallback(*event);
                }
            }
        };

        if (m_WrapMode != AnimationWrapMode::Loop || length <= 0.0f)
        {
            dispatchRange(NormalizeTime(clip, previousTime, m_WrapMode), NormalizeTime(clip, currentTime, m_WrapMode), false);
            return;
        }

        const float start = NormalizeTime(clip, previousTime, AnimationWrapMode::Loop);
        const float end = NormalizeTime(clip, currentTime, AnimationWrapMode::Loop);
        const int64_t startCycle = static_cast<int64_t>(std::floor(previousTime / length));
        const int64_t endCycle = static_cast<int64_t>(std::floor(currentTime / length));
        if (startCycle == endCycle)
        {
            dispatchRange(start, end, false);
            return;
        }

        if (forward)
        {
            dispatchRange(start, length, false);
            const int64_t fullCycles = std::min<int64_t>(endCycle - startCycle - 1, 16);
            for (int64_t cycle = 0; cycle < fullCycles; cycle++)
                dispatchRange(0.0f, length, true);
            dispatchRange(0.0f, end, true);
        }
        else
        {
            dispatchRange(start, 0.0f, false);
            const int64_t fullCycles = std::min<int64_t>(startCycle - endCycle - 1, 16);
            for (int64_t cycle = 0; cycle < fullCycles; cycle++)
                dispatchRange(length, 0.0f, true);
            dispatchRange(length, end, true);
        }
    }

    void AnimationPlayer::CalculateRootMotion(const AnimationClip& clip, float previousTime, float currentTime)
    {
        m_RootMotionDelta = IdentityTransform();
        const RootMotionCurves& rootMotion = clip.GetRootMotion();
        if (rootMotion.IsEmpty())
            return;

        const auto samplePosition = [&](float time) {
            const float localTime = NormalizeTime(clip, time, m_WrapMode);
            glm::vec3 value = rootMotion.Position.IsEmpty() ? glm::vec3(0.0f) : rootMotion.Position.Evaluate(localTime);
            if (m_WrapMode == AnimationWrapMode::Loop && clip.GetLength() > 0.0f && !rootMotion.Position.IsEmpty())
            {
                const int64_t cycle = static_cast<int64_t>(std::floor(time / clip.GetLength()));
                const glm::vec3 start = rootMotion.Position.Evaluate(0.0f, AnimationWrapMode::Clamp);
                const glm::vec3 end = rootMotion.Position.Evaluate(clip.GetLength(), AnimationWrapMode::Clamp);
                value += static_cast<float>(cycle) * (end - start);
            }
            return value;
        };
        const auto quaternionPower = [](glm::quat value, int64_t exponent) {
            if (exponent < 0)
            {
                value = glm::inverse(value);
                exponent = -exponent;
            }
            glm::quat result(1.0f, 0.0f, 0.0f, 0.0f);
            while (exponent > 0)
            {
                if ((exponent & 1) != 0)
                    result = glm::normalize(result * value);
                value = glm::normalize(value * value);
                exponent >>= 1;
            }
            return result;
        };
        const auto sampleRotation = [&](float time) {
            const float localTime = NormalizeTime(clip, time, m_WrapMode);
            glm::quat value = rootMotion.Rotation.IsEmpty() ? glm::quat(1.0f, 0.0f, 0.0f, 0.0f) : rootMotion.Rotation.Evaluate(localTime);
            if (m_WrapMode == AnimationWrapMode::Loop && clip.GetLength() > 0.0f && !rootMotion.Rotation.IsEmpty())
            {
                const int64_t cycle = static_cast<int64_t>(std::floor(time / clip.GetLength()));
                const glm::quat start = rootMotion.Rotation.Evaluate(0.0f, AnimationWrapMode::Clamp);
                const glm::quat end = rootMotion.Rotation.Evaluate(clip.GetLength(), AnimationWrapMode::Clamp);
                const glm::quat loopDelta = glm::normalize(glm::inverse(start) * end);
                value = glm::normalize(start * quaternionPower(loopDelta, cycle) * glm::inverse(start) * value);
            }
            return value;
        };

        const glm::vec3 previousPosition = samplePosition(previousTime);
        const glm::vec3 currentPosition = samplePosition(currentTime);
        const glm::quat previousRotation = sampleRotation(previousTime);
        const glm::quat currentRotation = sampleRotation(currentTime);
        m_RootMotionDelta =
          Transform(currentPosition - previousPosition, glm::normalize(glm::inverse(previousRotation) * currentRotation), glm::vec3(1.0f));
    }

    void AnimationPlayer::EvaluateMorphWeights(const AnimationClip& clip, float time, const MeshMorph& morph, Vector<float>& weights) const
    {
        weights.resize(morph.GetChannelCount());
        std::fill(weights.begin(), weights.end(), 0.0f);
        for (uint32_t channel = 0; channel < morph.GetChannelCount(); channel++)
        {
            const int32_t track = clip.FindMorphTrack(morph.GetChannel(channel)->GetName());
            if (track >= 0)
                weights[channel] = clip.SampleMorphWeight(static_cast<uint32_t>(track), time, m_WrapMode);
        }
    }

    void AnimationPlayer::Evaluate(const Ref<Skeleton>& skeleton, const Ref<MeshMorph>& morph)
    {
        if (!m_Clip)
            return;

        if (skeleton)
        {
            if (m_CurrentPose.GetSkeleton() != skeleton)
            {
                m_CurrentPose.SetSkeleton(skeleton);
                m_FadePose.SetSkeleton(skeleton);
                m_OutputPose.SetSkeleton(skeleton);
            }
            m_CurrentPose.Evaluate(*m_Clip, m_Time, m_WrapMode);
            if (m_Clip->IsAdditive())
            {
                m_OutputPose.ResetToBindPose();
                SkeletonPose::ApplyAdditive(m_OutputPose, m_CurrentPose, 1.0f, m_CurrentPose);
            }
            if (m_FadeFromClip)
            {
                m_FadePose.Evaluate(*m_FadeFromClip, m_FadeFromTime, m_WrapMode);
                if (m_FadeFromClip->IsAdditive())
                {
                    m_OutputPose.ResetToBindPose();
                    SkeletonPose::ApplyAdditive(m_OutputPose, m_FadePose, 1.0f, m_FadePose);
                }
                const float fade = m_FadeDuration > 0.0f ? glm::clamp(m_FadeElapsed / m_FadeDuration, 0.0f, 1.0f) : 1.0f;
                SkeletonPose::Blend(m_FadePose, m_CurrentPose, fade, m_OutputPose);
            }
            else
            {
                SkeletonPose::Blend(m_CurrentPose, m_CurrentPose, 0.0f, m_OutputPose);
            }
        }

        if (morph)
        {
            EvaluateMorphWeights(*m_Clip, m_Time, *morph, m_CurrentMorphWeights);
            m_MorphWeights.resize(morph->GetChannelCount());
            if (m_FadeFromClip)
            {
                EvaluateMorphWeights(*m_FadeFromClip, m_FadeFromTime, *morph, m_FadeMorphWeights);
                const float fade = m_FadeDuration > 0.0f ? glm::clamp(m_FadeElapsed / m_FadeDuration, 0.0f, 1.0f) : 1.0f;
                for (uint32_t channel = 0; channel < m_MorphWeights.size(); channel++)
                    m_MorphWeights[channel] = glm::mix(m_FadeMorphWeights[channel], m_CurrentMorphWeights[channel], fade);
            }
            else
            {
                std::copy(m_CurrentMorphWeights.begin(), m_CurrentMorphWeights.end(), m_MorphWeights.begin());
            }
        }
        else
        {
            m_MorphWeights.clear();
        }
    }

    void AnimationPlayer::Update(float deltaTime, const Ref<Skeleton>& skeleton, const Ref<MeshMorph>& morph)
    {
        m_RootMotionDelta = IdentityTransform();
        if (!m_Clip)
            return;

        if (m_State == AnimationPlaybackState::Playing && std::isfinite(deltaTime))
        {
            m_PreviousTime = m_Time;
            const float step = deltaTime * m_Speed;
            m_Time += step;
            if (m_FadeFromClip)
            {
                m_FadeFromTime += step;
                m_FadeElapsed += glm::abs(deltaTime);
            }
            DispatchEvents(*m_Clip, m_PreviousTime, m_Time);
            CalculateRootMotion(*m_Clip, m_PreviousTime, m_Time);

            if (m_WrapMode == AnimationWrapMode::Clamp && ((m_Speed >= 0.0f && m_Time >= m_Clip->GetLength()) || (m_Speed < 0.0f && m_Time <= 0.0f)))
            {
                m_Time = glm::clamp(m_Time, 0.0f, m_Clip->GetLength());
                m_State = AnimationPlaybackState::Stopped;
            }
            if (m_FadeFromClip && m_FadeElapsed >= m_FadeDuration)
                m_FadeFromClip = nullptr;
        }

        Evaluate(skeleton, morph);
    }
} // namespace Crowny

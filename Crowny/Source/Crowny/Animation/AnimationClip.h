#pragma once

#include "Crowny/Animation/AnimationCurve.h"
#include "Crowny/Assets/Asset.h"

namespace Crowny
{
    class AnimationClip : public Asset
    {
    public:
        AnimationClip(const AnimationCurve<float>& curve);
        virtual ~AnimationClip() override = default;

        static Ref<AnimationClip> Create(const AnimationCurve<float>& curve);

    private:
        uint32_t m_SampleRate;
        float m_Length;
        AnimationCurve<glm::vec3> m_RootMotion;
        AnimationCurve<float> m_Curves;
    };
} // namespace Crowny
#include "cwpch.h"

#include "Crowny/Animation/AnimationClip.h"

namespace Crowny
{
    AnimationClip::AnimationClip(const AnimationCurve<float>& curve) : m_Curves(curve) {}
    Ref<AnimationClip> AnimationClip::Create(const AnimationCurve<float>& curve) { return CreateRef<AnimationClip>(curve); }
} // namespace Crowny
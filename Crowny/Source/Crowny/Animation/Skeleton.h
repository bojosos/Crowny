#pragma once

namespace Crowny
{
	struct BoneProps
	{
        String Name;
        uint32_t ParentIdx;
        glm::mat4 LocalTransform;
        glm::mat4 BindPoseInverse;
	};

	struct AnimationState
    {
    };

    struct SkeletonBone
    {
        String Name;
        uint32_t ParentIdx;
    };

    class Skeleton
    {
    public:
    private:
        Vector<glm::mat4> m_BoneTransforms;
        Vector<glm::mat4> m_BindPoseInverse;
        Vector<SkeletonBone> m_Bones;
    };
}
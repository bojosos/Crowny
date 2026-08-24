#pragma once

#include "Crowny/Animation/AnimationClip.h"
#include "Crowny/Animation/MorphAnimation.h"
#include "Crowny/Common/HashedString.h"
#include "Crowny/Common/Math.h"
#include "Crowny/Common/RefCounted.h"
#include "Crowny/Math/AABox.h"
#include "Crowny/Math/SphereBounds.h"

namespace Crowny
{
    class MeshData;

    static constexpr uint32_t INVALID_BONE_INDEX = std::numeric_limits<uint32_t>::max();

    struct SkeletonBone
    {
        String Name;
        uint32_t ParentIdx = INVALID_BONE_INDEX;
        Transform LocalBindPose;
        glm::mat4 InverseBindPose{ 1.0f };
    };

    using BoneProps = SkeletonBone;

    /** Per-bone influence. A zero entry preserves the destination pose. */
    class SkeletonMask
    {
    public:
        SkeletonMask() = default;
        explicit SkeletonMask(uint32_t boneCount, float weight = 1.0f) : m_Weights(boneCount, weight) {}

        void Resize(uint32_t boneCount, float weight = 1.0f) { m_Weights.resize(boneCount, weight); }
        void SetWeight(uint32_t boneIndex, float weight);
        float GetWeight(uint32_t boneIndex) const;
        const Vector<float>& GetWeights() const { return m_Weights; }

    private:
        Vector<float> m_Weights;
    };

    /** Immutable hierarchy and bind-pose data shared by animation instances. */
    class Skeleton : public RefCounted
    {
    public:
        Skeleton() = default;
        explicit Skeleton(Vector<SkeletonBone> bones);

        uint32_t GetBoneCount() const { return static_cast<uint32_t>(m_Bones.size()); }
        const SkeletonBone& GetBone(uint32_t index) const { return m_Bones[index]; }
        const Vector<SkeletonBone>& GetBones() const { return m_Bones; }
        int32_t FindBone(StringView name) const;
        uint32_t GetRootBoneIndex() const;
        Transform CalculateBoneTransform(uint32_t index) const;
        bool IsValid() const { return m_Valid; }

        static Ref<Skeleton> Create(Vector<SkeletonBone> bones);

    private:
        void RebuildLookup();
        bool ValidateHierarchy() const;

        Vector<SkeletonBone> m_Bones;
        UnorderedMap<String, uint32_t, StringHash, StringEqual> m_BoneLookup;
        bool m_Valid = true;
    };

    /** Reusable animation-instance pose. Evaluation and matrix rebuilding allocate only when the skeleton changes. */
    class SkeletonPose
    {
    public:
        SkeletonPose() = default;
        explicit SkeletonPose(const Ref<Skeleton>& skeleton) { SetSkeleton(skeleton); }

        void SetSkeleton(const Ref<Skeleton>& skeleton);
        const Ref<Skeleton>& GetSkeleton() const { return m_Skeleton; }
        uint32_t GetBoneCount() const { return static_cast<uint32_t>(m_LocalTransforms.size()); }
        const Vector<Transform>& GetLocalTransforms() const { return m_LocalTransforms; }
        const Vector<glm::mat4>& GetGlobalTransforms() const { return m_GlobalTransforms; }
        const Vector<glm::mat4>& GetSkinningMatrices() const { return m_SkinningMatrices; }
        Transform& GetLocalTransform(uint32_t index) { return m_LocalTransforms[index]; }
        const Transform& GetLocalTransform(uint32_t index) const { return m_LocalTransforms[index]; }

        void ResetToBindPose();
        void Evaluate(const AnimationClip& clip, float time, AnimationWrapMode wrapMode = AnimationWrapMode::Loop,
                      const SkeletonMask* mask = nullptr);
        void RebuildMatrices();

        static void Blend(const SkeletonPose& first, const SkeletonPose& second, float weight, SkeletonPose& output,
                          const SkeletonMask* mask = nullptr);
        static void ApplyAdditive(const SkeletonPose& base, const SkeletonPose& additive, float weight, SkeletonPose& output,
                                  const SkeletonMask* mask = nullptr);

    private:
        void RebuildBone(uint32_t index);

        Ref<Skeleton> m_Skeleton;
        Vector<Transform> m_LocalTransforms;
        Vector<glm::mat4> m_GlobalTransforms;
        Vector<glm::mat4> m_SkinningMatrices;
        Vector<uint8_t> m_GlobalState;
    };

    /** Persistent CPU morph and skinning workspace used by animated mesh instances. */
    class MeshDeformer : public RefCounted
    {
    public:
        bool Initialize(const Ref<MeshData>& meshData, const Ref<Skeleton>& skeleton = nullptr, const Ref<MeshMorph>& morph = nullptr);
        bool Deform(const SkeletonPose* pose, const Vector<float>& morphWeights = {});

        const Ref<MeshData>& GetOutputMeshData() const { return m_OutputMeshData; }
        const Ref<Skeleton>& GetSkeleton() const { return m_Skeleton; }
        const Ref<MeshMorph>& GetMorph() const { return m_Morph; }
        const SphereBounds& GetSphereBounds() const { return m_SphereBounds; }
        const AABox& GetBounds() const { return m_Bounds; }
        bool IsInitialized() const { return m_OutputMeshData != nullptr; }

    private:
        Ref<MeshData> m_OutputMeshData;
        Ref<Skeleton> m_Skeleton;
        Ref<MeshMorph> m_Morph;
        Vector<glm::vec3> m_BasePositions;
        Vector<glm::vec3> m_BaseNormals;
        Vector<glm::vec3> m_BaseTangents;
        Vector<glm::vec3> m_BaseBitangents;
        Vector<glm::vec3> m_PreviousPositions;
        Vector<glm::vec3> m_Positions;
        Vector<glm::vec3> m_Normals;
        Vector<glm::vec3> m_Tangents;
        Vector<glm::vec3> m_Bitangents;
        Vector<glm::vec4> m_BlendWeights;
        Vector<glm::ivec4> m_BlendIndices;
        AABox m_Bounds;
        SphereBounds m_SphereBounds;
    };
} // namespace Crowny

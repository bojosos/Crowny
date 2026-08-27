#include "cwpch.h"

#include "Crowny/Animation/Skeleton.h"
#include "Crowny/Renderer/Mesh.h"

#include <glm/gtx/matrix_operation.hpp>

namespace Crowny
{
    namespace
    {
        Transform BlendTransform(const Transform& first, const Transform& second, float weight)
        {
            const float clampedWeight = glm::clamp(weight, 0.0f, 1.0f);
            return Transform(glm::mix(first.GetPosition(), second.GetPosition(), clampedWeight),
                             glm::normalize(glm::slerp(first.GetRotation(), second.GetRotation(), clampedWeight)),
                             glm::mix(first.GetScale(), second.GetScale(), clampedWeight));
        }
    } // namespace

    void SkeletonMask::SetWeight(uint32_t boneIndex, float weight)
    {
        if (boneIndex >= m_Weights.size())
            m_Weights.resize(boneIndex + 1, 1.0f);
        m_Weights[boneIndex] = glm::clamp(weight, 0.0f, 1.0f);
    }

    float SkeletonMask::GetWeight(uint32_t boneIndex) const
    {
        return boneIndex < m_Weights.size() ? glm::clamp(m_Weights[boneIndex], 0.0f, 1.0f) : 1.0f;
    }

    Skeleton::Skeleton(Vector<SkeletonBone> bones) : m_Bones(std::move(bones))
    {
        RebuildLookup();
        m_Valid = ValidateHierarchy();
    }

    Ref<Skeleton> Skeleton::Create(Vector<SkeletonBone> bones) { return CreateRef<Skeleton>(std::move(bones)); }

    void Skeleton::RebuildLookup()
    {
        m_BoneLookup.clear();
        for (uint32_t index = 0; index < m_Bones.size(); index++)
            m_BoneLookup.insert_or_assign(m_Bones[index].Name, index);
    }

    bool Skeleton::ValidateHierarchy() const
    {
        Vector<uint8_t> state(m_Bones.size(), 0);
        std::function<bool(uint32_t)> visit = [&](uint32_t index) {
            if (state[index] == 2)
                return true;
            if (state[index] == 1)
                return false;
            state[index] = 1;
            const uint32_t parent = m_Bones[index].ParentIdx;
            if (parent != INVALID_BONE_INDEX && (parent >= m_Bones.size() || parent == index || !visit(parent)))
                return false;
            state[index] = 2;
            return true;
        };
        for (uint32_t index = 0; index < m_Bones.size(); index++)
        {
            if (m_Bones[index].Name.empty() || !visit(index))
                return false;
        }
        return true;
    }

    int32_t Skeleton::FindBone(StringView name) const
    {
        const auto found = m_BoneLookup.find(name);
        return found == m_BoneLookup.end() ? -1 : static_cast<int32_t>(found->second);
    }

    uint32_t Skeleton::GetRootBoneIndex() const
    {
        for (uint32_t index = 0; index < m_Bones.size(); index++)
        {
            if (m_Bones[index].ParentIdx == INVALID_BONE_INDEX)
                return index;
        }
        return INVALID_BONE_INDEX;
    }

    Transform Skeleton::CalculateBoneTransform(uint32_t index) const
    {
        if (index >= m_Bones.size())
            return Transform();
        Transform result = m_Bones[index].LocalBindPose;
        uint32_t parent = m_Bones[index].ParentIdx;
        while (parent != INVALID_BONE_INDEX && parent < m_Bones.size())
        {
            result.MakeWorld(m_Bones[parent].LocalBindPose);
            parent = m_Bones[parent].ParentIdx;
        }
        return result;
    }

    void SkeletonPose::SetSkeleton(const Ref<Skeleton>& skeleton)
    {
        m_Skeleton = skeleton;
        const uint32_t count = skeleton ? skeleton->GetBoneCount() : 0;
        m_LocalTransforms.resize(count);
        m_GlobalTransforms.resize(count, glm::mat4(1.0f));
        m_SkinningMatrices.resize(count, glm::mat4(1.0f));
        m_GlobalState.resize(count, 0);
        ResetToBindPose();
    }

    void SkeletonPose::ResetToBindPose()
    {
        if (!m_Skeleton)
            return;
        for (uint32_t index = 0; index < m_Skeleton->GetBoneCount(); index++)
            m_LocalTransforms[index] = m_Skeleton->GetBone(index).LocalBindPose;
        RebuildMatrices();
    }

    void SkeletonPose::Evaluate(const AnimationClip& clip, float time, AnimationWrapMode wrapMode, const SkeletonMask* mask)
    {
        if (!m_Skeleton)
            return;

        const Transform additiveIdentity(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));

        for (uint32_t index = 0; index < m_Skeleton->GetBoneCount(); index++)
        {
            const SkeletonBone& bone = m_Skeleton->GetBone(index);
            const int32_t trackIndex = clip.FindTransformTrack(bone.Name);
            if (trackIndex < 0)
            {
                m_LocalTransforms[index] = clip.IsAdditive() ? additiveIdentity : bone.LocalBindPose;
                continue;
            }

            const Transform& fallback = clip.IsAdditive() ? additiveIdentity : bone.LocalBindPose;
            const Transform sampled = clip.SampleTransform(static_cast<uint32_t>(trackIndex), time, wrapMode, fallback);
            const float weight = mask != nullptr ? mask->GetWeight(index) : 1.0f;
            m_LocalTransforms[index] = weight >= 1.0f ? sampled : BlendTransform(fallback, sampled, weight);
        }
        RebuildMatrices();
    }

    void SkeletonPose::RebuildBone(uint32_t index)
    {
        if (m_GlobalState[index] == 2)
            return;
        if (m_GlobalState[index] == 1)
            return;
        m_GlobalState[index] = 1;

        const SkeletonBone& bone = m_Skeleton->GetBone(index);
        const glm::mat4 local = m_LocalTransforms[index].GetMatrix();
        if (bone.ParentIdx != INVALID_BONE_INDEX && bone.ParentIdx < GetBoneCount())
        {
            RebuildBone(bone.ParentIdx);
            m_GlobalTransforms[index] = m_GlobalTransforms[bone.ParentIdx] * local;
        }
        else
        {
            m_GlobalTransforms[index] = local;
        }
        m_SkinningMatrices[index] = m_GlobalTransforms[index] * bone.InverseBindPose;
        m_GlobalState[index] = 2;
    }

    void SkeletonPose::RebuildMatrices()
    {
        if (!m_Skeleton)
            return;
        std::fill(m_GlobalState.begin(), m_GlobalState.end(), 0);
        for (uint32_t index = 0; index < GetBoneCount(); index++)
            RebuildBone(index);
    }

    void SkeletonPose::Blend(const SkeletonPose& first, const SkeletonPose& second, float weight, SkeletonPose& output, const SkeletonMask* mask)
    {
        if (!first.m_Skeleton || first.m_Skeleton != second.m_Skeleton)
            return;
        if (output.m_Skeleton != first.m_Skeleton)
            output.SetSkeleton(first.m_Skeleton);

        for (uint32_t index = 0; index < first.GetBoneCount(); index++)
        {
            const float boneWeight = weight * (mask != nullptr ? mask->GetWeight(index) : 1.0f);
            output.m_LocalTransforms[index] = BlendTransform(first.m_LocalTransforms[index], second.m_LocalTransforms[index], boneWeight);
        }
        output.RebuildMatrices();
    }

    void SkeletonPose::ApplyAdditive(const SkeletonPose& base, const SkeletonPose& additive, float weight, SkeletonPose& output,
                                     const SkeletonMask* mask)
    {
        if (!base.m_Skeleton || base.m_Skeleton != additive.m_Skeleton)
            return;
        if (output.m_Skeleton != base.m_Skeleton)
            output.SetSkeleton(base.m_Skeleton);

        for (uint32_t index = 0; index < base.GetBoneCount(); index++)
        {
            const float boneWeight = glm::clamp(weight * (mask != nullptr ? mask->GetWeight(index) : 1.0f), 0.0f, 1.0f);
            const Transform& baseTransform = base.m_LocalTransforms[index];
            const Transform& delta = additive.m_LocalTransforms[index];
            output.m_LocalTransforms[index] =
              Transform(baseTransform.GetPosition() + delta.GetPosition() * boneWeight,
                        glm::normalize(baseTransform.GetRotation() * glm::slerp(glm::quat(1.0f, 0.0f, 0.0f, 0.0f), delta.GetRotation(), boneWeight)),
                        baseTransform.GetScale() * glm::mix(glm::vec3(1.0f), delta.GetScale(), boneWeight));
        }
        output.RebuildMatrices();
    }

    bool MeshDeformer::Initialize(const Ref<MeshData>& meshData, const Ref<Skeleton>& skeleton, const Ref<MeshMorph>& morph)
    {
        if (!meshData || !meshData->GetBufferLayout().HasAttribute(VertexAttribute::Position))
            return false;

        m_HasDeformationState = false;
        m_DeformationStateSettled = false;
        m_LastDeformChanged = false;
        m_LastSkinningMatrices.clear();
        m_LastMorphWeights.clear();
        m_Skeleton = skeleton;
        m_Morph = morph;
        m_BasePositions = meshData->GetPositions();
        m_BaseNormals = meshData->GetBufferLayout().HasAttribute(VertexAttribute::Normal) ? meshData->GetNormals() : Vector<glm::vec3>{};
        m_BaseTangents = meshData->GetBufferLayout().HasAttribute(VertexAttribute::Tangent) ? meshData->GetTangents() : Vector<glm::vec3>{};
        m_BaseBitangents = meshData->GetBufferLayout().HasAttribute(VertexAttribute::Bitangent) ? meshData->GetBitangents() : Vector<glm::vec3>{};
        m_PreviousPositions = m_BasePositions;
        m_Positions = m_BasePositions;
        m_Normals = m_BaseNormals;
        m_Tangents = m_BaseTangents;
        m_Bitangents = m_BaseBitangents;

        const bool hasSkinning = skeleton && meshData->GetBufferLayout().HasAttribute(VertexAttribute::BlendWeights) &&
                                 meshData->GetBufferLayout().HasAttribute(VertexAttribute::BlendIndices);
        if (hasSkinning)
        {
            m_BlendWeights.resize(meshData->GetVertexCount());
            m_BlendIndices.resize(meshData->GetVertexCount());
            meshData->GetVertexData(VertexAttribute::BlendWeights, m_BlendWeights.data(),
                                    static_cast<uint32_t>(m_BlendWeights.size() * sizeof(glm::vec4)));
            meshData->GetVertexData(VertexAttribute::BlendIndices, m_BlendIndices.data(),
                                    static_cast<uint32_t>(m_BlendIndices.size() * sizeof(glm::ivec4)));
        }
        else
        {
            m_BlendWeights.clear();
            m_BlendIndices.clear();
        }

        BufferLayout outputLayout;
        for (const BufferElement& element : meshData->GetBufferLayout())
            outputLayout.AddBufferElement(element);
        if (!outputLayout.HasAttribute(VertexAttribute::PreviousPosition))
            outputLayout.AddBufferElement({ ShaderDataType::Float3, VertexAttribute::PreviousPosition });
        m_OutputMeshData =
          MeshData::Create(meshData->GetVertexCount(), meshData->GetIndexCount(), outputLayout, meshData->GetIndexType());
        std::memcpy(m_OutputMeshData->GetIndexData(), meshData->GetIndexData(), meshData->GetIndexBufferSize());
        const auto& sourceElements = meshData->GetBufferLayout().GetElements();
        const auto& outputElements = outputLayout.GetElements();
        for (uint32_t elementIndex = 0; elementIndex < sourceElements.size(); elementIndex++)
        {
            const BufferElement& sourceElement = sourceElements[elementIndex];
            const BufferElement& outputElement = outputElements[elementIndex];
            const uint8_t* source = meshData->GetElementData(sourceElement);
            uint8_t* destination = m_OutputMeshData->GetElementData(outputElement);
            for (uint32_t vertex = 0; vertex < meshData->GetVertexCount(); vertex++)
            {
                std::memcpy(destination, source, sourceElement.Size);
                source += meshData->GetBufferLayout().GetStride();
                destination += outputLayout.GetStride();
            }
        }
        m_OutputMeshData->SetVertexData(VertexAttribute::PreviousPosition, m_PreviousPositions.data(),
                                        static_cast<uint32_t>(m_PreviousPositions.size() * sizeof(glm::vec3)));
        m_OutputMeshData->CalculateBounds(m_Bounds, m_SphereBounds);
        return true;
    }

    bool MeshDeformer::Deform(const SkeletonPose* pose, const Vector<float>& morphWeights)
    {
        if (!m_OutputMeshData)
        {
            m_LastDeformChanged = false;
            return false;
        }

        const Vector<glm::mat4>* skinningMatrices =
          pose != nullptr && pose->GetSkeleton() == m_Skeleton && !m_BlendWeights.empty() ? &pose->GetSkinningMatrices() : nullptr;
        const Vector<float>* effectiveMorphWeights = m_Morph ? &morphWeights : nullptr;
        bool matricesMatch = skinningMatrices == nullptr ? m_LastSkinningMatrices.empty()
                                                         : skinningMatrices->size() == m_LastSkinningMatrices.size();
        if (matricesMatch && skinningMatrices != nullptr)
        {
            for (uint32_t matrixIndex = 0; matrixIndex < skinningMatrices->size() && matricesMatch; matrixIndex++)
            {
                for (uint32_t column = 0; column < 4 && matricesMatch; column++)
                {
                    for (uint32_t row = 0; row < 4; row++)
                    {
                        if ((*skinningMatrices)[matrixIndex][column][row] != m_LastSkinningMatrices[matrixIndex][column][row])
                        {
                            matricesMatch = false;
                            break;
                        }
                    }
                }
            }
        }
        const bool morphWeightsMatch = effectiveMorphWeights == nullptr ? m_LastMorphWeights.empty()
                                                                         : *effectiveMorphWeights == m_LastMorphWeights;
        if (m_HasDeformationState && matricesMatch && morphWeightsMatch)
        {
            if (!m_DeformationStateSettled)
            {
                std::copy(m_Positions.begin(), m_Positions.end(), m_PreviousPositions.begin());
                m_OutputMeshData->SetVertexData(VertexAttribute::PreviousPosition, m_PreviousPositions.data(),
                                                static_cast<uint32_t>(m_PreviousPositions.size() * sizeof(glm::vec3)));
                m_DeformationStateSettled = true;
                m_LastDeformChanged = true;
            }
            else
                m_LastDeformChanged = false;
            return true;
        }

        std::copy(m_Positions.begin(), m_Positions.end(), m_PreviousPositions.begin());
        if (m_Morph)
            m_Morph->Apply(morphWeights, m_BasePositions, m_BaseNormals, m_Positions, m_Normals);
        else
        {
            std::copy(m_BasePositions.begin(), m_BasePositions.end(), m_Positions.begin());
            std::copy(m_BaseNormals.begin(), m_BaseNormals.end(), m_Normals.begin());
        }
        std::copy(m_BaseTangents.begin(), m_BaseTangents.end(), m_Tangents.begin());
        std::copy(m_BaseBitangents.begin(), m_BaseBitangents.end(), m_Bitangents.begin());

        if (skinningMatrices != nullptr)
        {
            const Vector<glm::mat4>& matrices = *skinningMatrices;
            for (uint32_t vertex = 0; vertex < m_Positions.size(); vertex++)
            {
                glm::mat4 skin(0.0f);
                float totalWeight = 0.0f;
                for (uint32_t influence = 0; influence < 4; influence++)
                {
                    const int32_t boneIndex = m_BlendIndices[vertex][influence];
                    const float boneWeight = m_BlendWeights[vertex][influence];
                    if (boneWeight <= 0.0f || boneIndex < 0 || static_cast<uint32_t>(boneIndex) >= matrices.size())
                        continue;
                    skin += matrices[boneIndex] * boneWeight;
                    totalWeight += boneWeight;
                }
                if (totalWeight <= std::numeric_limits<float>::epsilon())
                    continue;
                if (glm::abs(totalWeight - 1.0f) > 0.0001f)
                    skin /= totalWeight;

                m_Positions[vertex] = glm::vec3(skin * glm::vec4(m_Positions[vertex], 1.0f));
                const glm::mat3 directionTransform(skin);
                if (vertex < m_Normals.size())
                {
                    const float determinant = glm::determinant(directionTransform);
                    const glm::mat3 normalTransform = glm::abs(determinant) > std::numeric_limits<float>::epsilon()
                                                        ? glm::transpose(glm::inverse(directionTransform))
                                                        : directionTransform;
                    const glm::vec3 normal = normalTransform * m_Normals[vertex];
                    if (glm::dot(normal, normal) > std::numeric_limits<float>::epsilon())
                        m_Normals[vertex] = glm::normalize(normal);
                }
                if (vertex < m_Tangents.size())
                {
                    const glm::vec3 tangent = directionTransform * m_Tangents[vertex];
                    if (glm::dot(tangent, tangent) > std::numeric_limits<float>::epsilon())
                        m_Tangents[vertex] = glm::normalize(tangent);
                }
                if (vertex < m_Bitangents.size())
                {
                    const glm::vec3 bitangent = directionTransform * m_Bitangents[vertex];
                    if (glm::dot(bitangent, bitangent) > std::numeric_limits<float>::epsilon())
                        m_Bitangents[vertex] = glm::normalize(bitangent);
                }
            }
        }

        m_OutputMeshData->SetPositions(m_Positions);
        m_OutputMeshData->SetVertexData(VertexAttribute::PreviousPosition, m_PreviousPositions.data(),
                                        static_cast<uint32_t>(m_PreviousPositions.size() * sizeof(glm::vec3)));
        if (!m_Normals.empty())
            m_OutputMeshData->SetNormals(m_Normals);
        if (!m_Tangents.empty())
            m_OutputMeshData->SetTangents(m_Tangents);
        if (!m_Bitangents.empty())
            m_OutputMeshData->SetBitangents(m_Bitangents);
        m_OutputMeshData->CalculateBounds(m_Bounds, m_SphereBounds);
        if (skinningMatrices != nullptr)
            m_LastSkinningMatrices = *skinningMatrices;
        else
            m_LastSkinningMatrices.clear();
        if (effectiveMorphWeights != nullptr)
            m_LastMorphWeights = *effectiveMorphWeights;
        else
            m_LastMorphWeights.clear();
        m_HasDeformationState = true;
        m_DeformationStateSettled = false;
        m_LastDeformChanged = true;
        return true;
    }
} // namespace Crowny

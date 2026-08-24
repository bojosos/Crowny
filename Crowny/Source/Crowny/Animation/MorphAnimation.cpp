#include "cwpch.h"

#include "Crowny/Animation/MorphAnimation.h"

namespace Crowny
{
    MorphShape::MorphShape(String name, float weight, Vector<MorphData> morphs)
      : m_Name(std::move(name)), m_Weight(std::isfinite(weight) ? weight : 1.0f), m_Morphs(std::move(morphs))
    {
        std::stable_sort(m_Morphs.begin(), m_Morphs.end(),
                         [](const MorphData& left, const MorphData& right) { return left.VertexIndex < right.VertexIndex; });
    }

    Ref<MorphShape> MorphShape::Create(String name, float weight, Vector<MorphData> morphs)
    {
        return CreateRef<MorphShape>(std::move(name), weight, std::move(morphs));
    }

    MorphChannel::MorphChannel(String name, Vector<Ref<MorphShape>> shapes) : m_Name(std::move(name)), m_Shapes(std::move(shapes))
    {
        m_Shapes.erase(std::remove(m_Shapes.begin(), m_Shapes.end(), nullptr), m_Shapes.end());
        std::stable_sort(m_Shapes.begin(), m_Shapes.end(),
                         [](const Ref<MorphShape>& left, const Ref<MorphShape>& right) { return left->GetWeight() < right->GetWeight(); });
    }

    Ref<MorphChannel> MorphChannel::Create(String name, Vector<Ref<MorphShape>> shapes)
    {
        return CreateRef<MorphChannel>(std::move(name), std::move(shapes));
    }

    MeshMorph::MeshMorph(Vector<Ref<MorphChannel>> channels, uint32_t vertexCount) : m_Channels(std::move(channels)), m_VertexCount(vertexCount)
    {
        m_Channels.erase(std::remove(m_Channels.begin(), m_Channels.end(), nullptr), m_Channels.end());
        RebuildLookup();
    }

    Ref<MeshMorph> MeshMorph::Create(Vector<Ref<MorphChannel>> channels, uint32_t vertexCount)
    {
        return CreateRef<MeshMorph>(std::move(channels), vertexCount);
    }

    void MeshMorph::RebuildLookup()
    {
        m_ChannelLookup.clear();
        for (uint32_t index = 0; index < m_Channels.size(); index++)
            m_ChannelLookup.insert_or_assign(m_Channels[index]->GetName(), index);
    }

    int32_t MeshMorph::FindChannel(StringView name) const
    {
        const auto found = m_ChannelLookup.find(name);
        return found == m_ChannelLookup.end() ? -1 : static_cast<int32_t>(found->second);
    }

    void MeshMorph::ApplyShape(const MorphShape& shape, float weight, Vector<glm::vec3>& positions, Vector<glm::vec3>& normals) const
    {
        if (glm::abs(weight) <= std::numeric_limits<float>::epsilon())
            return;

        for (const MorphData& vertex : shape.GetVertices())
        {
            if (vertex.VertexIndex >= positions.size())
                continue;
            positions[vertex.VertexIndex] += vertex.VertexTranslation * weight;
            if (vertex.VertexIndex < normals.size())
                normals[vertex.VertexIndex] += vertex.NormalTranslation * weight;
        }
    }

    void MeshMorph::Apply(const Vector<float>& channelWeights, const Vector<glm::vec3>& basePositions, const Vector<glm::vec3>& baseNormals,
                          Vector<glm::vec3>& outputPositions, Vector<glm::vec3>& outputNormals) const
    {
        outputPositions.assign(basePositions.begin(), basePositions.end());
        outputNormals.assign(baseNormals.begin(), baseNormals.end());

        const uint32_t channelCount = std::min(static_cast<uint32_t>(channelWeights.size()), GetChannelCount());
        for (uint32_t channelIndex = 0; channelIndex < channelCount; channelIndex++)
        {
            const MorphChannel& channel = *m_Channels[channelIndex];
            const auto& shapes = channel.GetShapes();
            if (shapes.empty())
                continue;

            const float weight = channelWeights[channelIndex];
            if (shapes.size() == 1)
            {
                const float threshold = shapes.front()->GetWeight();
                const float factor =
                  threshold > std::numeric_limits<float>::epsilon() ? glm::clamp(weight / threshold, 0.0f, 1.0f) : (weight > 0.0f ? 1.0f : 0.0f);
                ApplyShape(*shapes.front(), factor, outputPositions, outputNormals);
                continue;
            }

            if (weight <= shapes.front()->GetWeight())
            {
                const float threshold = shapes.front()->GetWeight();
                const float factor = threshold > std::numeric_limits<float>::epsilon() ? glm::clamp(weight / threshold, 0.0f, 1.0f) : 1.0f;
                ApplyShape(*shapes.front(), factor, outputPositions, outputNormals);
                continue;
            }

            if (weight >= shapes.back()->GetWeight())
            {
                ApplyShape(*shapes.back(), 1.0f, outputPositions, outputNormals);
                continue;
            }

            const auto upper = std::upper_bound(shapes.begin(), shapes.end(), weight,
                                                [](float value, const Ref<MorphShape>& shape) { return value < shape->GetWeight(); });
            const Ref<MorphShape>& right = *upper;
            const Ref<MorphShape>& left = *(upper - 1);
            const float range = right->GetWeight() - left->GetWeight();
            const float factor = range > std::numeric_limits<float>::epsilon() ? (weight - left->GetWeight()) / range : 1.0f;
            ApplyShape(*left, 1.0f - factor, outputPositions, outputNormals);
            ApplyShape(*right, factor, outputPositions, outputNormals);
        }

        for (glm::vec3& normal : outputNormals)
        {
            const float lengthSquared = glm::dot(normal, normal);
            if (lengthSquared > std::numeric_limits<float>::epsilon())
                normal *= glm::inversesqrt(lengthSquared);
        }
    }
} // namespace Crowny

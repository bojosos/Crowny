#pragma once

#include "Crowny/Common/HashedString.h"
#include "Crowny/Common/RefCounted.h"
#include "Crowny/Common/Types.h"

#include <glm/glm.hpp>

namespace Crowny
{
    /** Sparse difference between a mesh vertex and a morph target. */
    struct MorphData
    {
        glm::vec3 VertexTranslation{ 0.0f };
        glm::vec3 NormalTranslation{ 0.0f };
        uint32_t VertexIndex = 0;
    };

    class MorphShape : public RefCounted
    {
    public:
        MorphShape() = default;
        MorphShape(String name, float weight, Vector<MorphData> morphs);

        const String& GetName() const { return m_Name; }
        float GetWeight() const { return m_Weight; }
        const Vector<MorphData>& GetVertices() const { return m_Morphs; }

        static Ref<MorphShape> Create(String name, float weight, Vector<MorphData> morphs);

    private:
        String m_Name;
        float m_Weight = 1.0f;
        Vector<MorphData> m_Morphs;
    };

    /** Shapes blended sequentially as the channel weight moves from zero to one. */
    class MorphChannel : public RefCounted
    {
    public:
        MorphChannel() = default;
        MorphChannel(String name, Vector<Ref<MorphShape>> shapes);

        const String& GetName() const { return m_Name; }
        uint32_t GetShapeCount() const { return static_cast<uint32_t>(m_Shapes.size()); }
        const Ref<MorphShape>& GetShape(uint32_t index) const { return m_Shapes[index]; }
        const Vector<Ref<MorphShape>>& GetShapes() const { return m_Shapes; }

        static Ref<MorphChannel> Create(String name, Vector<Ref<MorphShape>> shapes);

    private:
        String m_Name;
        Vector<Ref<MorphShape>> m_Shapes;
    };

    class MeshMorph : public RefCounted
    {
    public:
        MeshMorph() = default;
        MeshMorph(Vector<Ref<MorphChannel>> channels, uint32_t vertexCount);

        uint32_t GetChannelCount() const { return static_cast<uint32_t>(m_Channels.size()); }
        uint32_t GetVertexCount() const { return m_VertexCount; }
        const Ref<MorphChannel>& GetChannel(uint32_t index) const { return m_Channels[index]; }
        const Vector<Ref<MorphChannel>>& GetChannels() const { return m_Channels; }
        int32_t FindChannel(StringView name) const;

        /**
         * Applies all channel weights additively. Output vectors retain capacity, so repeated evaluation does not allocate
         * after their first resize.
         */
        void Apply(const Vector<float>& channelWeights, const Vector<glm::vec3>& basePositions, const Vector<glm::vec3>& baseNormals,
                   Vector<glm::vec3>& outputPositions, Vector<glm::vec3>& outputNormals) const;

        static Ref<MeshMorph> Create(Vector<Ref<MorphChannel>> channels, uint32_t vertexCount);

    private:
        void RebuildLookup();
        void ApplyShape(const MorphShape& shape, float weight, Vector<glm::vec3>& positions, Vector<glm::vec3>& normals) const;

        Vector<Ref<MorphChannel>> m_Channels;
        uint32_t m_VertexCount = 0;
        UnorderedMap<String, uint32_t, StringHash, StringEqual> m_ChannelLookup;
    };

    // Compatibility names used by older importer code and serialized data.
    using SingleMorph = MorphShape;
    using FullMorph = MorphChannel;
} // namespace Crowny

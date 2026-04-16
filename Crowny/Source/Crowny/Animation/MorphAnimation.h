#pragma once

#include "Crowny/Common/RefCounted.h"

namespace Crowny
{
    struct MorphData
    {
        glm::vec3 VertexTranslation;
        glm::vec3 NormalTranslation;
        uint32_t VertexIndex;
    };

    class SingleMorph : public RefCounted
    {
    public:
        SingleMorph(const String& name, float weight, const Vector<MorphData>& morphs);

        static Ref<SingleMorph> Create(const String& name, float weight, const Vector<MorphData>& morphs);

    private:
        String m_Name;
        float m_Weight;
        Vector<MorphData> m_Morphs;
    };

    class FullMorph
    {
    public:
        FullMorph(const String& name, const Vector<SingleMorph>& morphs);

    private:
        String m_Name;
        Vector<SingleMorph> m_Morphs;
    };

    class MeshMorph : public RefCounted
    {
    public:
        MeshMorph(const Vector<FullMorph>& morphs, uint32_t vertexCount);

    private:
        Vector<FullMorph> m_Morphs;
        uint32_t m_VertexCount;
    };
} // namespace Crowny
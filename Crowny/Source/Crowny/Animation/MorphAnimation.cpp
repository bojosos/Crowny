#include "cwpch.h"

#include "Crowny/Animation/MorphAnimation.h"

namespace Crowny
{
    SingleMorph::SingleMorph(const String& name, const float weight, const Vector<MorphData>& morphs) : m_Name(name), m_Weight(weight), m_Morphs(morphs) {}

    Ref<SingleMorph> SingleMorph::Create(const String& name, const float weight, const Vector<MorphData>& morphs)
    {
        return CreateRef<SingleMorph>(name, weight, morphs);
    }

    FullMorph::FullMorph(const String& name, const Vector<SingleMorph>& morphs) : m_Name(name), m_Morphs(morphs) {}
    MeshMorph::MeshMorph(const Vector<FullMorph>& morphs, const uint32_t vertexCount) : m_Morphs(morphs), m_VertexCount(vertexCount) {}
} // namespace Crowny
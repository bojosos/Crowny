#pragma once

#include "Crowny/Renderer/ComputeMaterial.h"
#include "Crowny/Renderer/ClusteredLightGrid.h"

namespace Crowny
{
    class DecalGpuGrid
    {
    public:
        bool Build(const ClusteredLightGridDesc& desc, const glm::mat4& view, const glm::mat4& projection,
                   const Vector<glm::vec4>& bounds, uint32_t indexCapacity);
        const Ref<GenericGpuBuffer>& GetCells() const { return m_Cells; }
        const Ref<GenericGpuBuffer>& GetIndices() const { return m_Indices; }
        bool MatchesReference(const Vector<glm::uvec2>& cells, const Vector<uint32_t>& indices) const;

    private:
        ComputeMaterial m_Builder;
        Ref<GenericGpuBuffer> m_Cells, m_Indices, m_Bounds, m_Ranges, m_Cursors, m_Counters;
    };
}

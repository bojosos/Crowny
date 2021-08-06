#include "cwpch.h"

#include "Crowny/RenderAPI/TextureView.h"

namespace Crowny
{

    TextureView::TextureView(const TextureViewDesc& desc) : m_Desc(desc) {}

    size_t TextureView::HashFunction::operator()(const TextureViewDesc& key) const
    {
        size_t hash = 0;
        HashCombine(hash, key.MostDetailedMip, key.NumMips, key.FirstArraySlice, key.NumArraySlices, key.Usage);

        return hash;
    }

    bool TextureView::EqualFunction::operator()(const TextureViewDesc& lhs, const TextureViewDesc& rhs) const
    {
        return lhs.MostDetailedMip == rhs.MostDetailedMip && lhs.NumMips == rhs.NumMips &&
               lhs.FirstArraySlice == rhs.FirstArraySlice && lhs.NumArraySlices == rhs.NumArraySlices &&
               lhs.Usage == rhs.Usage;
    }

} // namespace Crowny
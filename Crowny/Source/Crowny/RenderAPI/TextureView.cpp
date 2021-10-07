#include "cwpch.h"

#include "Crowny/RenderAPI/TextureView.h"

namespace Crowny
{

    TextureView::TextureView(const TextureViewDesc& desc) : m_Desc(desc) {}

    size_t TextureView::HashFunction::operator()(const TextureViewDesc& key) const
    {
        size_t hash = 0;
        HashCombine(hash, key.MostDetailedMip, key.NumMips, key.FirstFace, key.NumFaces, key.Usage);

        return hash;
    }

    bool TextureView::EqualFunction::operator()(const TextureViewDesc& lhs, const TextureViewDesc& rhs) const
    {
        return lhs.MostDetailedMip == rhs.MostDetailedMip && lhs.NumMips == rhs.NumMips &&
               lhs.FirstFace == rhs.FirstFace && lhs.NumFaces == rhs.NumFaces && lhs.Usage == rhs.Usage;
    }

} // namespace Crowny
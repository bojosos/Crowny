#include "cwpch.h"

#include "Crowny/Renderer/Font.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Utils/PixelUtils.h"

#include "Crowny/Renderer/MSDFdata.h"

#include <FontGeometry.h>
#include <GlyphGeometry.h>
#include <msdf-atlas-gen.h>

namespace Crowny
{
    AssetHandle<Font> Font::s_DefaultFont;

    Font::Font(MSDFData* msdfData, const Ref<Texture>& atlasTexture, uint32_t tabWidth)
      : m_MSDFData(msdfData), m_AtlasTexture(atlasTexture), m_TabWidth(std::max(1U, tabWidth))
    {
    }

    Font::~Font() { delete m_MSDFData; }

    AssetHandle<Font> Font::GetDefaultFont() { return s_DefaultFont; }

    void Font::SetDefaultFont(const AssetHandle<Font>& font) { s_DefaultFont = font; }

    bool Font::IsValid() const
    {
        return m_MSDFData != nullptr && m_AtlasTexture != nullptr && m_AtlasTexture->GetWidth() > 0 && m_AtlasTexture->GetHeight() > 0;
    }

    const msdfgen::FontMetrics* Font::GetMetrics() const
    {
        return m_MSDFData != nullptr ? &m_MSDFData->FontGeometry.getMetrics() : nullptr;
    }

    const msdf_atlas::GlyphGeometry* Font::GetGlyph(char32_t codePoint, char32_t* resolvedCodePoint) const
    {
        if (m_MSDFData == nullptr)
            return nullptr;

        const msdf_atlas::FontGeometry& geometry = m_MSDFData->FontGeometry;
        const Array<char32_t, 4> candidates = { codePoint, 0xFFFD, 0x25A1, U'?' };
        for (char32_t candidate : candidates)
        {
            if (const msdf_atlas::GlyphGeometry* glyph = geometry.getGlyph(candidate))
            {
                if (resolvedCodePoint != nullptr)
                    *resolvedCodePoint = candidate;
                return glyph;
            }
        }
        return nullptr;
    }

    double Font::GetAdvance(char32_t codePoint, char32_t nextCodePoint, bool useKerning) const
    {
        char32_t resolvedCodePoint = 0;
        const msdf_atlas::GlyphGeometry* glyph = GetGlyph(codePoint, &resolvedCodePoint);
        if (glyph == nullptr)
            return 0.0;

        double advance = glyph->getAdvance();
        if (useKerning && nextCodePoint != 0)
        {
            char32_t resolvedNextCodePoint = 0;
            if (GetGlyph(nextCodePoint, &resolvedNextCodePoint) != nullptr)
                m_MSDFData->FontGeometry.getAdvance(advance, resolvedCodePoint, resolvedNextCodePoint);
        }
        return advance;
    }

} // namespace Crowny

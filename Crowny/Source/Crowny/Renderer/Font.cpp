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

    Font::Font() = default;

    Font::Font(Scope<MSDFData> msdfData, const Ref<Texture>& atlasTexture, uint32_t tabWidth, float atlasPixelRange)
      : m_MSDFData(std::move(msdfData)), m_AtlasTexture(atlasTexture), m_TabWidth(std::clamp(tabWidth, 1U, 64U)),
        m_AtlasPixelRange(std::isfinite(atlasPixelRange) && atlasPixelRange > 0.0f ? atlasPixelRange : 2.0f)
    {
    }

    Font::~Font() = default;

    AssetHandle<Font> Font::GetDefaultFont() { return s_DefaultFont; }

    void Font::SetDefaultFont(const AssetHandle<Font>& font) { s_DefaultFont = font; }

    bool Font::IsValid() const
    {
        FontMetrics metrics;
        return m_MSDFData != nullptr && m_AtlasTexture != nullptr && m_AtlasTexture->GetWidth() > 0 && m_AtlasTexture->GetHeight() > 0 &&
               GetGlyphCount() > 0 && TryGetMetrics(metrics);
    }

    uint32_t Font::GetAtlasWidth() const { return m_AtlasTexture != nullptr ? m_AtlasTexture->GetWidth() : 0; }

    uint32_t Font::GetAtlasHeight() const { return m_AtlasTexture != nullptr ? m_AtlasTexture->GetHeight() : 0; }

    const msdfgen::FontMetrics* Font::GetMetrics() const { return m_MSDFData != nullptr ? &m_MSDFData->FontGeometry.getMetrics() : nullptr; }

    bool Font::TryGetMetrics(FontMetrics& metrics) const
    {
        metrics = {};
        const msdfgen::FontMetrics* source = GetMetrics();
        if (source == nullptr || !std::isfinite(source->emSize) || !std::isfinite(source->ascenderY) || !std::isfinite(source->descenderY) ||
            !std::isfinite(source->lineHeight) || !std::isfinite(source->underlineY) || !std::isfinite(source->underlineThickness) ||
            !std::isfinite(source->strikethroughY) || source->ascenderY <= source->descenderY || source->lineHeight <= 0.0)
            return false;

        metrics.EmSize = source->emSize;
        metrics.Ascender = source->ascenderY;
        metrics.Descender = source->descenderY;
        metrics.LineHeight = source->lineHeight;
        metrics.UnderlineY = source->underlineY;
        metrics.UnderlineThickness = source->underlineThickness;
        metrics.StrikethroughY = source->strikethroughY;
        return true;
    }

    size_t Font::GetGlyphCount() const { return m_MSDFData != nullptr ? m_MSDFData->FontGeometry.getGlyphs().size() : 0; }

    bool Font::HasGlyph(char32_t codePoint) const { return FindGlyph(codePoint) != nullptr; }

    const msdf_atlas::GlyphGeometry* Font::FindGlyph(char32_t codePoint) const
    {
        return m_MSDFData != nullptr ? m_MSDFData->FontGeometry.getGlyph(codePoint) : nullptr;
    }

    Font::GlyphLookup Font::ResolveGlyph(char32_t codePoint, bool useFallbacks) const
    {
        auto findExact = [codePoint](const Font& font) -> GlyphLookup {
            if (!font.IsValid())
                return {};
            const msdf_atlas::GlyphGeometry* glyph = font.FindGlyph(codePoint);
            return glyph != nullptr ? GlyphLookup{ &font, glyph, codePoint } : GlyphLookup{};
        };

        if (GlyphLookup lookup = findExact(*this))
            return lookup;
        if (useFallbacks)
        {
            for (const AssetHandle<Font>& fallback : m_FallbackFonts)
            {
                if (fallback)
                {
                    if (GlyphLookup lookup = findExact(*fallback))
                        return lookup;
                }
            }
        }

        constexpr Array<char32_t, 3> replacementCodePoints = { 0xFFFD, 0x25A1, U'?' };
        for (char32_t replacement : replacementCodePoints)
        {
            auto findReplacement = [replacement](const Font& font) -> GlyphLookup {
                if (!font.IsValid())
                    return {};
                const msdf_atlas::GlyphGeometry* glyph = font.FindGlyph(replacement);
                return glyph != nullptr ? GlyphLookup{ &font, glyph, replacement } : GlyphLookup{};
            };

            if (GlyphLookup lookup = findReplacement(*this))
                return lookup;
            if (!useFallbacks)
                continue;
            for (const AssetHandle<Font>& fallback : m_FallbackFonts)
            {
                if (fallback)
                {
                    if (GlyphLookup lookup = findReplacement(*fallback))
                        return lookup;
                }
            }
        }
        return {};
    }

    CharacterInfo Font::GetCharacterInfo(char32_t codePoint, bool useFallbacks) const
    {
        CharacterInfo info;
        info.RequestedCodePoint = codePoint;

        const GlyphLookup lookup = ResolveGlyph(codePoint, useFallbacks);
        if (!lookup)
            return info;

        info.SourceFont = lookup.SourceFont;
        info.ResolvedCodePoint = lookup.ResolvedCodePoint;
        info.GlyphIndex = lookup.Glyph->getIndex();
        info.Advance = lookup.Glyph->getAdvance();
        lookup.Glyph->getQuadPlaneBounds(info.PlaneLeft, info.PlaneBottom, info.PlaneRight, info.PlaneTop);
        lookup.Glyph->getQuadAtlasBounds(info.AtlasLeft, info.AtlasBottom, info.AtlasRight, info.AtlasTop);
        info.Whitespace = lookup.Glyph->isWhitespace();
        info.Valid = true;
        return info;
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

    void Font::SetFallbackFonts(const Vector<AssetHandle<Font>>& fonts)
    {
        m_FallbackFonts.clear();
        m_FallbackFonts.reserve(std::min(fonts.size(), MAX_FALLBACK_FONTS));
        for (const AssetHandle<Font>& font : fonts)
        {
            if (!AddFallbackFont(font) && m_FallbackFonts.size() == MAX_FALLBACK_FONTS)
                break;
        }
    }

    bool Font::AddFallbackFont(const AssetHandle<Font>& font)
    {
        if (!font || font.Get() == this || m_FallbackFonts.size() >= MAX_FALLBACK_FONTS)
            return false;
        const auto duplicate = std::find_if(m_FallbackFonts.begin(), m_FallbackFonts.end(),
                                            [&font](const AssetHandle<Font>& candidate) { return candidate.Get() == font.Get(); });
        if (duplicate != m_FallbackFonts.end())
            return false;
        m_FallbackFonts.push_back(font);
        return true;
    }

} // namespace Crowny

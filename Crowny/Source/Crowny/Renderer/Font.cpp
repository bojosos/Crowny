#include "cwpch.h"

#include "Crowny/Renderer/Font.h"

#include "Crowny/Assets/AssetManager.h"
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
    namespace
    {
        constexpr size_t MAX_FALLBACK_SEARCH_FONTS = 64;

        class FallbackGlyphTraversal
        {
        public:
            void Reset() noexcept { m_VisitedCount = 0; }

            bool TryVisit(const Font* font) noexcept
            {
                for (size_t index = 0; index < m_VisitedCount; index++)
                {
                    if (m_Visited[index] == font)
                        return false;
                }
                if (m_VisitedCount == m_Visited.size())
                    return false;

                m_Visited[m_VisitedCount++] = font;
                return true;
            }

        private:
            Array<const Font*, MAX_FALLBACK_SEARCH_FONTS> m_Visited;
            size_t m_VisitedCount = 0;
        };

        Font::GlyphLookup FindGlyphInFallbackGraph(const Font& font, char32_t codePoint, FallbackGlyphTraversal& traversal)
        {
            if (!traversal.TryVisit(&font))
                return {};

            if (font.IsValid())
            {
                if (const msdf_atlas::GlyphGeometry* glyph = font.FindGlyph(codePoint))
                    return { &font, glyph, codePoint };
            }

            for (const AssetHandle<Font>& fallback : font.GetFallbackFonts())
            {
                if (fallback)
                {
                    if (Font::GlyphLookup lookup = FindGlyphInFallbackGraph(*fallback, codePoint, traversal))
                        return lookup;
                }
            }
            return {};
        }

        UUID FindFallbackUuid(const Font& font, const Font* source, UnorderedSet<const Font*>& visited, size_t& remainingFonts)
        {
            if (source == nullptr || remainingFonts == 0 || !visited.insert(&font).second)
                return {};
            remainingFonts--;

            for (const AssetHandle<Font>& fallback : font.GetFallbackFonts())
            {
                if (!fallback)
                    continue;
                if (fallback.Get() == source)
                    return fallback.GetUUID();
                const UUID nested = FindFallbackUuid(*fallback, source, visited, remainingFonts);
                if (!nested.Empty())
                    return nested;
            }
            return {};
        }
    } // namespace

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
        FallbackGlyphTraversal traversal;
        auto find = [this, useFallbacks, &traversal](char32_t candidate) {
            if (!useFallbacks)
            {
                if (!IsValid())
                    return GlyphLookup{};
                const msdf_atlas::GlyphGeometry* glyph = FindGlyph(candidate);
                return glyph != nullptr ? GlyphLookup{ this, glyph, candidate } : GlyphLookup{};
            }

            traversal.Reset();
            return FindGlyphInFallbackGraph(*this, candidate, traversal);
        };

        if (GlyphLookup lookup = find(codePoint))
            return lookup;

        constexpr Array<char32_t, 3> replacementCodePoints = { 0xFFFD, 0x25A1, U'?' };
        for (char32_t replacement : replacementCodePoints)
        {
            if (GlyphLookup lookup = find(replacement))
                return lookup;
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
        ClearFallbackFonts();
        m_FallbackFonts.reserve(std::min(fonts.size(), MAX_FALLBACK_FONTS));
        m_FallbackFontIds.reserve(std::min(fonts.size(), MAX_FALLBACK_FONTS));
        for (const AssetHandle<Font>& font : fonts)
        {
            if (!AddFallbackFont(font) && m_FallbackFontIds.size() == MAX_FALLBACK_FONTS)
                break;
        }
    }

    bool Font::AddFallbackFont(const AssetHandle<Font>& font)
    {
        if (!font || !font.HasUUID() || font.Get() == this)
            return false;
        const UUID& fontId = font.GetUUID();
        const bool hasId = std::find(m_FallbackFontIds.begin(), m_FallbackFontIds.end(), fontId) != m_FallbackFontIds.end();
        if (!hasId && m_FallbackFontIds.size() >= MAX_FALLBACK_FONTS)
            return false;
        const auto duplicate = std::find_if(m_FallbackFonts.begin(), m_FallbackFonts.end(), [&font, &fontId](const AssetHandle<Font>& candidate) {
            return candidate.Get() == font.Get() || candidate.GetUUID() == fontId;
        });
        if (duplicate != m_FallbackFonts.end())
            return false;

        UnorderedSet<const Font*> visited;
        if (font->ReferencesFont(this, visited))
            return false;
        m_FallbackFonts.push_back(font);
        if (!hasId)
            m_FallbackFontIds.push_back(fontId);
        return true;
    }

    void Font::SetFallbackFontIds(const Vector<UUID>& fontIds)
    {
        ClearFallbackFonts();
        m_FallbackFontIds.reserve(std::min(fontIds.size(), MAX_FALLBACK_FONTS));
        for (const UUID& fontId : fontIds)
        {
            if (fontId.Empty() || std::find(m_FallbackFontIds.begin(), m_FallbackFontIds.end(), fontId) != m_FallbackFontIds.end())
                continue;
            m_FallbackFontIds.push_back(fontId);
            if (m_FallbackFontIds.size() == MAX_FALLBACK_FONTS)
                break;
        }
    }

    bool Font::LoadFallbackFonts()
    {
        if (m_FallbackFontIds.empty())
        {
            m_FallbackFonts.clear();
            return true;
        }
        if (AssetManager::TryGet() == nullptr)
            return false;

        Vector<AssetHandle<Font>> previousFonts = std::move(m_FallbackFonts);
        m_FallbackFonts.clear();
        Vector<UUID> retainedIds;
        retainedIds.reserve(m_FallbackFontIds.size());
        bool allLoaded = true;

        for (const UUID& fontId : m_FallbackFontIds)
        {
            AssetHandle<Font> font;
            const auto previous = std::find_if(previousFonts.begin(), previousFonts.end(),
                                               [&fontId](const AssetHandle<Font>& candidate) { return candidate.GetUUID() == fontId; });
            if (previous != previousFonts.end() && *previous)
                font = *previous;
            else
            {
                const AssetHandle<Asset> asset = AssetManager::TryGet()->LoadFromUUID(fontId, false);
                if (asset && asset->GetAssetType() != AssetType::Font)
                {
                    CW_ENGINE_ERROR("Discarding a font fallback that references a non-font asset.");
                    allLoaded = false;
                    continue;
                }
                font = static_asset_cast<Font>(asset);
            }

            if (!font)
            {
                retainedIds.push_back(fontId);
                allLoaded = false;
                continue;
            }
            if (font.Get() == this)
            {
                CW_ENGINE_ERROR("Discarding a font fallback that references itself.");
                allLoaded = false;
                continue;
            }

            UnorderedSet<const Font*> visited;
            if (font->ReferencesFont(this, visited))
            {
                CW_ENGINE_ERROR("Discarding a font fallback that creates a cycle.");
                allLoaded = false;
                continue;
            }
            m_FallbackFonts.push_back(font);
            retainedIds.push_back(fontId);
        }
        m_FallbackFontIds = std::move(retainedIds);
        return allLoaded;
    }

    void Font::ClearFallbackFonts()
    {
        m_FallbackFonts.clear();
        m_FallbackFontIds.clear();
    }

    UUID Font::FindFallbackFontUUID(const Font* font) const
    {
        UnorderedSet<const Font*> visited;
        size_t remainingFonts = MAX_FALLBACK_SEARCH_FONTS;
        return FindFallbackUuid(*this, font, visited, remainingFonts);
    }

    bool Font::ReferencesFont(const Font* font, UnorderedSet<const Font*>& visited) const
    {
        if (this == font)
            return true;
        if (!visited.insert(this).second)
            return false;
        for (const AssetHandle<Font>& fallback : m_FallbackFonts)
        {
            if (fallback && fallback->ReferencesFont(font, visited))
                return true;
        }
        return false;
    }

} // namespace Crowny

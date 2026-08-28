#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Assets/AssetHandle.h"

namespace msdf_atlas
{
    class GlyphGeometry;
}

namespace msdfgen
{
    struct FontMetrics;
}

namespace Crowny
{
    class Font;
    struct MSDFData;

    struct FontMetrics
    {
        double EmSize = 0.0;
        double Ascender = 0.0;
        double Descender = 0.0;
        double LineHeight = 0.0;
        double UnderlineY = 0.0;
        double UnderlineThickness = 0.0;
        double StrikethroughY = 0.0;
    };

    enum class CharsetRange
    {
        ASCII,
        ExtendedASCII,
        LowerASCII,
        UpperASCII,
        NumbersAndSymbols,
        SymbolRange,
        DecimalRange,
        HexRange,
        Count
    };

    struct CharacterInfo
    {
        const Font* SourceFont = nullptr;
        char32_t RequestedCodePoint = 0;
        char32_t ResolvedCodePoint = 0;
        int32_t GlyphIndex = -1;
        double Advance = 0.0;
        double PlaneLeft = 0.0;
        double PlaneBottom = 0.0;
        double PlaneRight = 0.0;
        double PlaneTop = 0.0;
        double AtlasLeft = 0.0;
        double AtlasBottom = 0.0;
        double AtlasRight = 0.0;
        double AtlasTop = 0.0;
        bool Whitespace = false;
        bool Valid = false;
    };

    struct FontDesc
    {
    };

    class Font : public Asset
    {
    public:
        struct GlyphLookup
        {
            const Font* SourceFont = nullptr;
            const msdf_atlas::GlyphGeometry* Glyph = nullptr;
            char32_t ResolvedCodePoint = 0;

            explicit operator bool() const { return SourceFont != nullptr && Glyph != nullptr; }
        };

        Font();
        Font(Scope<MSDFData> msdfData, const Ref<Texture>& atlasTexture, uint32_t tabWidth = 4, float atlasPixelRange = 2.0f);
        ~Font();

        enum class AtlasDimensionsConstraint
        {
            POWER_OF_TWO_SQUARE,
            POWER_OF_TWO_RECTANGLE,
            MULTIPLE_OF_FOUR_SQUARE,
            EVEN_SQUARE,
            SQUARE,
            COUNT
        };

        Ref<Texture> GetAtlasTexture() const { return m_AtlasTexture; }
        uint32_t GetTabWidth() const { return m_TabWidth; }
        uint32_t GetAtlasWidth() const;
        uint32_t GetAtlasHeight() const;
        float GetAtlasPixelRange() const { return m_AtlasPixelRange; }

        bool IsValid() const;
        const msdfgen::FontMetrics* GetMetrics() const;
        bool TryGetMetrics(FontMetrics& metrics) const;
        size_t GetGlyphCount() const;
        bool HasGlyph(char32_t codePoint) const;
        const msdf_atlas::GlyphGeometry* FindGlyph(char32_t codePoint) const;
        GlyphLookup ResolveGlyph(char32_t codePoint, bool useFallbacks = true) const;
        CharacterInfo GetCharacterInfo(char32_t codePoint, bool useFallbacks = true) const;
        const msdf_atlas::GlyphGeometry* GetGlyph(char32_t codePoint, char32_t* resolvedCodePoint = nullptr) const;
        double GetAdvance(char32_t codePoint, char32_t nextCodePoint = 0, bool useKerning = true) const;

        void SetFallbackFonts(const Vector<AssetHandle<Font>>& fonts);
        bool AddFallbackFont(const AssetHandle<Font>& font);
        void ClearFallbackFonts() { m_FallbackFonts.clear(); }
        const Vector<AssetHandle<Font>>& GetFallbackFonts() const { return m_FallbackFonts; }

        virtual AssetType GetAssetType() const override { return AssetType::Font; }
        static AssetType GetStaticType() { return AssetType::Font; }

        static AssetHandle<Font> GetDefaultFont();
        static void SetDefaultFont(const AssetHandle<Font>& font);

    private:
        CW_SERIALIZABLE(Font);

    private:
        static constexpr size_t MAX_FALLBACK_FONTS = 8;
        static AssetHandle<Font> s_DefaultFont;
        Scope<MSDFData> m_MSDFData;
        Ref<Texture> m_AtlasTexture;
        uint32_t m_TabWidth = 4;
        float m_AtlasPixelRange = 2.0f;
        Vector<AssetHandle<Font>> m_FallbackFonts;
    };

} // namespace Crowny

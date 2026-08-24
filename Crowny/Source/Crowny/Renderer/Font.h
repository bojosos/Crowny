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
    struct MSDFData;

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
    };

    struct FontDesc
    {
    };

    class Font : public Asset
    {
    public:
        Font() = default;
        Font(MSDFData* msdfData, const Ref<Texture>& atlasTexture, uint32_t tabWidth = 4);
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

        const MSDFData* GetMSDFData() const { return m_MSDFData; }
        Ref<Texture> GetAtlasTexture() const { return m_AtlasTexture; }
        uint32_t GetTabWidth() const { return m_TabWidth; }

        bool IsValid() const;
        const msdfgen::FontMetrics* GetMetrics() const;
        const msdf_atlas::GlyphGeometry* GetGlyph(char32_t codePoint, char32_t* resolvedCodePoint = nullptr) const;
        double GetAdvance(char32_t codePoint, char32_t nextCodePoint = 0, bool useKerning = true) const;

        virtual AssetType GetAssetType() const override { return AssetType::Font; }
        static AssetType GetStaticType() { return AssetType::Font; }

        static AssetHandle<Font> GetDefaultFont();
        static void SetDefaultFont(const AssetHandle<Font>& font);

    private:
        CW_SERIALIZABLE(Font);

    private:
        static AssetHandle<Font> s_DefaultFont;
        MSDFData* m_MSDFData = nullptr;
        Ref<Texture> m_AtlasTexture;
        uint32_t m_TabWidth = 4;
    };

} // namespace Crowny

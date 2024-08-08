#pragma once

#include "Crowny/Assets/Asset.h"

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

    struct CharacterInfo {

    };

    struct FontDesc
    {
    };

    class Font : public Asset
    {
    public:
        Font() = default;
        Font(MSDFData* msdfData, const Ref<Texture>& atlasTexture);
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

        virtual AssetType GetAssetType() const override { return AssetType::Font; }
        static AssetType GetStaticType() { return AssetType::Font; }

        static Ref<Font> GetDefault();

    private:
        CW_SERIALIZABLE(Font);

    private:
        MSDFData* m_MSDFData;
        Ref<Texture> m_AtlasTexture;
    };

} // namespace Crowny
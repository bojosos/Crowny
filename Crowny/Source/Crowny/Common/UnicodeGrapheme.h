#pragma once

#include <cstdint>

namespace Crowny
{
    /** Unicode Grapheme_Cluster_Break values used by extended grapheme segmentation. */
    enum class GraphemeBreakProperty : uint8_t
    {
        Other,
        CR,
        LF,
        Control,
        Extend,
        ZWJ,
        RegionalIndicator,
        Prepend,
        SpacingMark,
        L,
        V,
        T,
        LV,
        LVT
    };

    /** Unicode Indic_Conjunct_Break values used by grapheme rule GB9c. */
    enum class IndicConjunctBreakProperty : uint8_t
    {
        None,
        Consonant,
        Extend,
        Linker
    };

    namespace UnicodeGrapheme
    {
        inline constexpr const char* DATA_VERSION = "17.0.0";

        GraphemeBreakProperty GetBreakProperty(char32_t codePoint);
        IndicConjunctBreakProperty GetIndicConjunctBreakProperty(char32_t codePoint);
        bool IsExtendedPictographic(char32_t codePoint);
        bool IsSpacingMark(char32_t codePoint);
    } // namespace UnicodeGrapheme
} // namespace Crowny

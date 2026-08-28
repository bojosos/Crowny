#include "cwpch.h"

#include "Crowny/Common/UnicodeGrapheme.h"

#include <cstddef>

namespace Crowny
{
    namespace
    {
        struct GraphemePropertyRange
        {
            char32_t First;
            char32_t Last;
            GraphemeBreakProperty Property;
        };

        struct IndicPropertyRange
        {
            char32_t First;
            char32_t Last;
            IndicConjunctBreakProperty Property;
        };

        struct CodePointRange
        {
            char32_t First;
            char32_t Last;
        };

#include "Crowny/Common/UnicodeGraphemeData.inl"

        template <typename Range, size_t Size, typename Property>
        Property FindProperty(const Range (&ranges)[Size], char32_t codePoint, Property fallback)
        {
            size_t low = 0;
            size_t high = Size;
            while (low < high)
            {
                const size_t middle = low + (high - low) / 2;
                const Range& range = ranges[middle];
                if (codePoint < range.First)
                    high = middle;
                else if (codePoint > range.Last)
                    low = middle + 1;
                else
                    return range.Property;
            }
            return fallback;
        }

        template <size_t Size> bool Contains(const CodePointRange (&ranges)[Size], char32_t codePoint)
        {
            size_t low = 0;
            size_t high = Size;
            while (low < high)
            {
                const size_t middle = low + (high - low) / 2;
                const CodePointRange& range = ranges[middle];
                if (codePoint < range.First)
                    high = middle;
                else if (codePoint > range.Last)
                    low = middle + 1;
                else
                    return true;
            }
            return false;
        }
    } // namespace

    GraphemeBreakProperty UnicodeGrapheme::GetBreakProperty(char32_t codePoint)
    {
        return FindProperty(GRAPHEME_BREAK_RANGES, codePoint, GraphemeBreakProperty::Other);
    }

    IndicConjunctBreakProperty UnicodeGrapheme::GetIndicConjunctBreakProperty(char32_t codePoint)
    {
        return FindProperty(INDIC_CONJUNCT_BREAK_RANGES, codePoint, IndicConjunctBreakProperty::None);
    }

    bool UnicodeGrapheme::IsExtendedPictographic(char32_t codePoint) { return Contains(EXTENDED_PICTOGRAPHIC_RANGES, codePoint); }

    bool UnicodeGrapheme::IsSpacingMark(char32_t codePoint) { return Contains(SPACING_MARK_RANGES, codePoint); }
} // namespace Crowny

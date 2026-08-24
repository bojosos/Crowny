#pragma once

#include "Crowny/Memory/FrameVector.h"

#include <glm/glm.hpp>

namespace msdf_atlas
{
    class GlyphGeometry;
}

namespace Crowny
{
    class Font;
    struct TextComponent;

    struct TextLayoutToken
    {
        char32_t CodePoint = 0;
        const msdf_atlas::GlyphGeometry* Glyph = nullptr;
        double Advance = 0.0;
        bool NewLine = false;
        bool WhiteSpace = false;
        bool Invisible = false;
        bool BreakAfter = false;
        bool CombiningMark = false;
        bool Renderable = false;
    };

    struct TextLayoutGlyph
    {
        char32_t CodePoint = 0;
        const msdf_atlas::GlyphGeometry* Glyph = nullptr;
        glm::vec2 PenPosition{ 0.0f };
        float Advance = 0.0f;
        uint32_t LineIndex = 0;
    };

    struct TextLayoutLine
    {
        size_t TokenStart = 0;
        size_t TokenEnd = 0;
        size_t RenderTokenEnd = 0;
        size_t FirstGlyph = 0;
        size_t GlyphCount = 0;
        float X = 0.0f;
        float Baseline = 0.0f;
        float Width = 0.0f;
        float NaturalWidth = 0.0f;
        uint32_t ExpandableGaps = 0;
        bool ParagraphEnd = false;
        bool Ellipsized = false;
    };

    struct TextLayoutResult
    {
        const TextLayoutGlyph* Glyphs = nullptr;
        const TextLayoutLine* Lines = nullptr;
        size_t GlyphCount = 0;
        size_t LineCount = 0;
        glm::vec2 Size{ 0.0f };
        float FontSize = 0.0f;
        float GlyphScale = 0.0f;
        float LineAdvance = 0.0f;
        bool Truncated = false;
        bool OverflowedHorizontally = false;
        bool OverflowedVertically = false;
    };

    struct TextLayoutFontData
    {
        double Ascender = 0.0;
        double Descender = 0.0;
        double LineHeight = 0.0;
        double SpaceAdvance = 0.0;
        double EllipsisAdvance = 0.0;
        uint32_t TabWidth = 4;
        const msdf_atlas::GlyphGeometry* EllipsisGlyph = nullptr;
    };

    class TextLayoutScratch
    {
    public:
        void Reset()
        {
            Tokens.Reset();
            Lines.Reset();
            Glyphs.Reset();
        }

        void Reserve(size_t characterCount)
        {
            Tokens.Reserve(characterCount);
            Glyphs.Reserve(characterCount);
            Lines.Reserve(std::min(characterCount + 1, size_t(64)));
        }

        FrameVector<TextLayoutToken> Tokens;
        FrameVector<TextLayoutLine> Lines;
        FrameVector<TextLayoutGlyph> Glyphs;
    };

    class TextLayout final
    {
    public:
        static TextLayoutResult Build(const TextComponent& component, const Font& font, TextLayoutScratch& scratch);
        static TextLayoutResult BuildPrepared(const TextComponent& component, const TextLayoutFontData& fontData, TextLayoutScratch& scratch);
    };
} // namespace Crowny

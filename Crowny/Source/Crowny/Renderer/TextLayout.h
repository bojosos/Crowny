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
        // Half-open byte range in the original UTF-8 string.
        size_t SourceByteStart = 0;
        size_t SourceByteEnd = 0;
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

    struct TextLayoutCaret
    {
        size_t SourceByteOffset = 0;
        glm::vec2 Position{ 0.0f };
        uint32_t LineIndex = 0;
    };

    struct TextLayoutLine
    {
        size_t TokenStart = 0;
        size_t TokenEnd = 0;
        size_t RenderTokenEnd = 0;
        size_t FirstGlyph = 0;
        size_t GlyphCount = 0;
        size_t FirstCaret = 0;
        size_t CaretCount = 0;
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
        const TextLayoutCaret* Carets = nullptr;
        size_t GlyphCount = 0;
        size_t LineCount = 0;
        size_t CaretCount = 0;
        glm::vec2 Size{ 0.0f };
        float FontSize = 0.0f;
        float GlyphScale = 0.0f;
        float LineAdvance = 0.0f;
        bool Truncated = false;
        bool OverflowedHorizontally = false;
        bool OverflowedVertically = false;
    };

    struct TextHitTestResult
    {
        size_t SourceByteOffset = 0;
        glm::vec2 CaretPosition{ 0.0f };
        uint32_t LineIndex = 0;
        bool Valid = false;
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
            Carets.Reset();
            SourceByteLength = 0;
        }

        void Reserve(size_t characterCount)
        {
            Tokens.Reserve(characterCount);
            Glyphs.Reserve(characterCount);
            Carets.Reserve(characterCount + 1);
            Lines.Reserve(std::min(characterCount + 1, size_t(64)));
        }

        FrameVector<TextLayoutToken> Tokens;
        FrameVector<TextLayoutLine> Lines;
        FrameVector<TextLayoutGlyph> Glyphs;
        FrameVector<TextLayoutCaret> Carets;
        size_t SourceByteLength = 0;
    };

    class TextLayout final
    {
    public:
        // Result pointers remain valid until scratch is reused.
        static TextLayoutResult Build(const TextComponent& component, const Font& font, TextLayoutScratch& scratch);
        // Prepared tokens must provide source byte ranges for meaningful hit-test offsets.
        static TextLayoutResult BuildPrepared(const TextComponent& component, const TextLayoutFontData& fontData, TextLayoutScratch& scratch);
        // Returns the closest visible caret in layout space, clamped to the laid-out text.
        static TextHitTestResult HitTest(const TextLayoutResult& layout, const glm::vec2& position);
    };
} // namespace Crowny

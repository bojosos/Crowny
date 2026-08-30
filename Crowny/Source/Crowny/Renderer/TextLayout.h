#pragma once

#include "Crowny/Memory/FrameVector.h"

#include <glm/glm.hpp>
#include <span>

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
        char32_t ResolvedCodePoint = 0;
        const msdf_atlas::GlyphGeometry* Glyph = nullptr;
        const Font* SourceFont = nullptr;
        // Half-open byte range in the original UTF-8 string.
        size_t SourceByteStart = 0;
        size_t SourceByteEnd = 0;
        // Half-open byte range of the containing grapheme cluster.
        size_t ClusterByteStart = 0;
        size_t ClusterByteEnd = 0;
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
        const Font* SourceFont = nullptr;
        glm::vec2 PenPosition{ 0.0f };
        float Advance = 0.0f;
        uint32_t LineIndex = 0;
    };

    struct TextLayoutFontRun
    {
        const Font* SourceFont = nullptr;
        size_t FirstGlyph = 0;
        size_t GlyphCount = 0;
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
        const TextLayoutFontRun* FontRuns = nullptr;
        size_t GlyphCount = 0;
        size_t LineCount = 0;
        size_t CaretCount = 0;
        size_t FontRunCount = 0;
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
        const Font* EllipsisSourceFont = nullptr;
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
            FontRuns.Reset();
            SourceByteLength = 0;
        }

        void Reserve(size_t characterCount)
        {
            Tokens.Reserve(characterCount);
            Glyphs.Reserve(characterCount);
            Carets.Reserve(characterCount + 1);
            FontRuns.Reserve(std::min(characterCount, size_t(16)));
            Lines.Reserve(std::min(characterCount + 1, size_t(64)));
        }

        FrameVector<TextLayoutToken> Tokens;
        FrameVector<TextLayoutLine> Lines;
        FrameVector<TextLayoutGlyph> Glyphs;
        FrameVector<TextLayoutCaret> Carets;
        FrameVector<TextLayoutFontRun> FontRuns;
        size_t SourceByteLength = 0;
    };

    class TextLayout final
    {
    public:
        // Replaces each malformed UTF-8 byte with U+FFFD and records source and grapheme-cluster byte ranges.
        static void DecodeUTF8(StringView text, TextLayoutScratch& scratch);
        // Result pointers remain valid until scratch is reused.
        static TextLayoutResult Build(const TextComponent& component, const Font& font, TextLayoutScratch& scratch);
        // Prepared tokens must provide source byte ranges for meaningful hit-test offsets.
        static TextLayoutResult BuildPrepared(const TextComponent& component, const TextLayoutFontData& fontData, TextLayoutScratch& scratch);
        // Returns the closest visible caret in layout space, clamped to the laid-out text.
        static TextHitTestResult HitTest(const TextLayoutResult& layout, const glm::vec2& position);
        // Synchronous query helpers reuse one scratch buffer per thread and return no views into it.
        static TextHitTestResult HitTest(const TextComponent& component, const Font& font, const glm::vec2& position);
        static TextHitTestResult HitTestPrepared(const TextComponent& component, const TextLayoutFontData& fontData,
                                                 std::span<const TextLayoutToken> tokens, size_t sourceByteLength, const glm::vec2& position);
    };
} // namespace Crowny

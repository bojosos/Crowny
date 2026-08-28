#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Crowny/Ecs/Components.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/TextLayout.h"

using namespace Crowny;

namespace
{
    constexpr TextLayoutFontData UNIT_LAYOUT_FONT{ 0.8, -0.2, 1.0, 1.0, 1.0, 4, nullptr };

    void AddLayoutToken(TextLayoutScratch& scratch, char32_t codePoint, bool whitespace = false, bool newLine = false, size_t sourceByteCount = 1)
    {
        TextLayoutToken& token = scratch.Tokens.Acquire();
        token = {};
        token.CodePoint = codePoint;
        token.SourceByteStart = scratch.SourceByteLength;
        scratch.SourceByteLength += sourceByteCount;
        token.SourceByteEnd = scratch.SourceByteLength;
        token.Advance = newLine ? 0.0 : 1.0;
        token.NewLine = newLine;
        token.WhiteSpace = whitespace;
        token.BreakAfter = whitespace;
        token.Renderable = !whitespace && !newLine;
    }

    TextComponent MakeUnitTextComponent()
    {
        TextComponent component;
        component.Text = "prepared";
        component.Size = 36.0f;
        component.VerticalAlignment = TextVerticalAlignment::Baseline;
        return component;
    }
} // namespace

TEST_CASE("Text layout decodes UTF-8 into code point and byte clusters", "[Text][Layout][Unicode]")
{
    TextLayoutScratch scratch;

    TextLayout::DecodeUTF8("A\xc3\xa9"
                           "e\xcc\x81\xf0\x9f\x99\x82",
                           scratch);

    REQUIRE(scratch.Tokens.Size() == 5);
    CHECK(scratch.SourceByteLength == 10);

    CHECK(scratch.Tokens[0].CodePoint == U'A');
    CHECK(scratch.Tokens[0].SourceByteStart == 0);
    CHECK(scratch.Tokens[0].SourceByteEnd == 1);
    CHECK(scratch.Tokens[0].ClusterByteStart == 0);
    CHECK(scratch.Tokens[0].ClusterByteEnd == 1);

    CHECK(scratch.Tokens[1].CodePoint == U'\u00e9');
    CHECK(scratch.Tokens[1].SourceByteStart == 1);
    CHECK(scratch.Tokens[1].SourceByteEnd == 3);
    CHECK(scratch.Tokens[1].ClusterByteStart == 1);
    CHECK(scratch.Tokens[1].ClusterByteEnd == 3);

    CHECK(scratch.Tokens[2].CodePoint == U'e');
    CHECK(scratch.Tokens[2].ClusterByteStart == 3);
    CHECK(scratch.Tokens[2].ClusterByteEnd == 6);
    CHECK(scratch.Tokens[3].CodePoint == U'\u0301');
    CHECK(scratch.Tokens[3].SourceByteStart == 4);
    CHECK(scratch.Tokens[3].SourceByteEnd == 6);
    CHECK(scratch.Tokens[3].ClusterByteStart == 3);
    CHECK(scratch.Tokens[3].ClusterByteEnd == 6);

    CHECK(scratch.Tokens[4].CodePoint == U'\U0001f642');
    CHECK(scratch.Tokens[4].SourceByteStart == 6);
    CHECK(scratch.Tokens[4].SourceByteEnd == 10);
    CHECK(scratch.Tokens[4].ClusterByteStart == 6);
    CHECK(scratch.Tokens[4].ClusterByteEnd == 10);
}

TEST_CASE("Text layout replaces each malformed UTF-8 byte deterministically", "[Text][Layout][Unicode]")
{
    const String malformed("\xf0\x28\x8c\x28", 4);
    TextLayoutScratch scratch;

    TextLayout::DecodeUTF8(malformed, scratch);

    REQUIRE(scratch.Tokens.Size() == 4);
    CHECK(scratch.Tokens[0].CodePoint == U'\ufffd');
    CHECK(scratch.Tokens[0].SourceByteStart == 0);
    CHECK(scratch.Tokens[0].SourceByteEnd == 1);
    CHECK(scratch.Tokens[1].CodePoint == U'(');
    CHECK(scratch.Tokens[1].SourceByteStart == 1);
    CHECK(scratch.Tokens[1].SourceByteEnd == 2);
    CHECK(scratch.Tokens[2].CodePoint == U'\ufffd');
    CHECK(scratch.Tokens[2].SourceByteStart == 2);
    CHECK(scratch.Tokens[2].SourceByteEnd == 3);
    CHECK(scratch.Tokens[3].CodePoint == U'(');
    CHECK(scratch.Tokens[3].SourceByteStart == 3);
    CHECK(scratch.Tokens[3].SourceByteEnd == 4);
}

TEST_CASE("Text layout groups regional indicators and emoji joiner sequences", "[Text][Layout][Unicode]")
{
    TextLayoutScratch scratch;
    const String text = "\xf0\x9f\x87\xa7\xf0\x9f\x87\xac"
                        "\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x92\xbb";

    TextLayout::DecodeUTF8(text, scratch);

    REQUIRE(scratch.Tokens.Size() == 5);
    CHECK(scratch.Tokens[0].CodePoint == U'\U0001f1e7');
    CHECK(scratch.Tokens[0].ClusterByteStart == 0);
    CHECK(scratch.Tokens[0].ClusterByteEnd == 8);
    CHECK(scratch.Tokens[1].CodePoint == U'\U0001f1ec');
    CHECK(scratch.Tokens[1].ClusterByteStart == 0);
    CHECK(scratch.Tokens[1].ClusterByteEnd == 8);
    CHECK(scratch.Tokens[2].CodePoint == U'\U0001f469');
    CHECK(scratch.Tokens[2].ClusterByteStart == 8);
    CHECK(scratch.Tokens[2].ClusterByteEnd == 19);
    CHECK(scratch.Tokens[3].CodePoint == U'\u200d');
    CHECK(scratch.Tokens[3].ClusterByteStart == 8);
    CHECK(scratch.Tokens[3].ClusterByteEnd == 19);
    CHECK(scratch.Tokens[4].CodePoint == U'\U0001f4bb');
    CHECK(scratch.Tokens[4].ClusterByteStart == 8);
    CHECK(scratch.Tokens[4].ClusterByteEnd == 19);
}

TEST_CASE("Text layout applies Unicode extended grapheme rules beyond Latin marks", "[Text][Layout][Unicode]")
{
    TextLayoutScratch scratch;

    SECTION("Hebrew combining marks extend their base")
    {
        TextLayout::DecodeUTF8("\xd7\x90\xd6\xb0x", scratch);

        REQUIRE(scratch.Tokens.Size() == 3);
        CHECK(scratch.Tokens[0].CodePoint == U'\u05d0');
        CHECK(scratch.Tokens[1].CodePoint == U'\u05b0');
        CHECK(scratch.Tokens[0].ClusterByteStart == 0);
        CHECK(scratch.Tokens[0].ClusterByteEnd == 4);
        CHECK(scratch.Tokens[1].ClusterByteStart == 0);
        CHECK(scratch.Tokens[1].ClusterByteEnd == 4);
        CHECK(scratch.Tokens[2].ClusterByteStart == 4);
        CHECK(scratch.Tokens[2].ClusterByteEnd == 5);
    }

    SECTION("ZWJ only joins a following extended pictograph")
    {
        TextLayout::DecodeUTF8("A\xe2\x80\x8d"
                               "B",
                               scratch);

        REQUIRE(scratch.Tokens.Size() == 3);
        CHECK(scratch.Tokens[0].ClusterByteStart == 0);
        CHECK(scratch.Tokens[0].ClusterByteEnd == 4);
        CHECK(scratch.Tokens[1].ClusterByteStart == 0);
        CHECK(scratch.Tokens[1].ClusterByteEnd == 4);
        CHECK(scratch.Tokens[2].ClusterByteStart == 4);
        CHECK(scratch.Tokens[2].ClusterByteEnd == 5);
    }

    SECTION("Indic virama conjuncts remain indivisible")
    {
        TextLayout::DecodeUTF8("\xe0\xa4\x95\xe0\xa5\x8d\xe0\xa4\x95", scratch);

        REQUIRE(scratch.Tokens.Size() == 3);
        for (const TextLayoutToken& token : scratch.Tokens)
        {
            CHECK(token.ClusterByteStart == 0);
            CHECK(token.ClusterByteEnd == 9);
        }
    }
}

TEST_CASE("Text layout groups Hangul Jamo and spacing-mark graphemes", "[Text][Layout][Unicode]")
{
    TextLayoutScratch scratch;
    const String text = "\xe1\x84\x80\xe1\x85\xa1\xe1\x86\xa8"
                        "\xe0\xa4\x95\xe0\xa4\xbe";

    TextLayout::DecodeUTF8(text, scratch);

    REQUIRE(scratch.Tokens.Size() == 5);
    for (size_t index = 0; index < 3; index++)
    {
        CHECK(scratch.Tokens[index].ClusterByteStart == 0);
        CHECK(scratch.Tokens[index].ClusterByteEnd == 9);
    }
    for (size_t index = 3; index < 5; index++)
    {
        CHECK(scratch.Tokens[index].ClusterByteStart == 9);
        CHECK(scratch.Tokens[index].ClusterByteEnd == 15);
    }
}

TEST_CASE("Text layout overlays combining marks without adding advance", "[Text][Layout][Unicode]")
{
    TextComponent component = MakeUnitTextComponent();
    component.Wrapping = false;
    TextLayoutScratch scratch;
    TextLayout::DecodeUTF8("e\xcc\x81x", scratch);
    for (size_t index = 0; index < scratch.Tokens.Size(); index++)
    {
        scratch.Tokens[index].Advance = 1.0;
        scratch.Tokens[index].Renderable = true;
    }

    const TextLayoutResult layout = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);

    REQUIRE(layout.GlyphCount == 3);
    CHECK(layout.Glyphs[0].PenPosition.x == 0.0f);
    CHECK(layout.Glyphs[1].PenPosition.x == 0.0f);
    CHECK(layout.Glyphs[2].PenPosition.x == 1.0f);
    CHECK(layout.Size.x == 2.0f);
}

TEST_CASE("Text layout preserves advance for spacing marks classified as grapheme extenders", "[Text][Layout][Unicode]")
{
    TextComponent component = MakeUnitTextComponent();
    component.Wrapping = false;
    TextLayoutScratch scratch;
    TextLayout::DecodeUTF8("\xe0\xa6\x95\xe0\xa6\xbe"
                           "x",
                           scratch);
    for (size_t index = 0; index < scratch.Tokens.Size(); index++)
    {
        scratch.Tokens[index].Advance = 1.0;
        scratch.Tokens[index].Renderable = true;
    }

    const TextLayoutResult layout = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);

    REQUIRE(layout.GlyphCount == 3);
    CHECK(layout.Glyphs[0].CodePoint == U'\u0995');
    CHECK(layout.Glyphs[1].CodePoint == U'\u09be');
    CHECK(layout.Glyphs[1].PenPosition.x == 1.0f);
    CHECK(layout.Glyphs[2].PenPosition.x == 2.0f);
    CHECK(layout.Size.x == 3.0f);
}

TEST_CASE("Text layout normalizes newline sequences without losing byte offsets", "[Text][Layout][Unicode]")
{
    TextLayoutScratch scratch;

    TextLayout::DecodeUTF8("\r\n\n\r", scratch);

    REQUIRE(scratch.Tokens.Size() == 3);
    for (size_t index = 0; index < scratch.Tokens.Size(); index++)
        CHECK(scratch.Tokens[index].NewLine);
    CHECK(scratch.Tokens[0].SourceByteStart == 0);
    CHECK(scratch.Tokens[0].SourceByteEnd == 2);
    CHECK(scratch.Tokens[1].SourceByteStart == 2);
    CHECK(scratch.Tokens[1].SourceByteEnd == 3);
    CHECK(scratch.Tokens[2].SourceByteStart == 3);
    CHECK(scratch.Tokens[2].SourceByteEnd == 4);
}

TEST_CASE("Prepared text layout preserves contiguous font ownership runs", "[Text][Layout][Fonts]")
{
    Font primary;
    Font fallback;
    TextComponent component = MakeUnitTextComponent();
    component.Wrapping = false;
    TextLayoutScratch scratch;
    AddLayoutToken(scratch, U'a');
    AddLayoutToken(scratch, U'\u03b2');
    AddLayoutToken(scratch, U'\u03b3');
    scratch.Tokens[0].SourceFont = &primary;
    scratch.Tokens[1].SourceFont = &fallback;
    scratch.Tokens[2].SourceFont = &fallback;

    const TextLayoutResult layout = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);

    REQUIRE(layout.GlyphCount == 3);
    CHECK(layout.Glyphs[0].SourceFont == &primary);
    CHECK(layout.Glyphs[1].SourceFont == &fallback);
    CHECK(layout.Glyphs[2].SourceFont == &fallback);
    REQUIRE(layout.FontRunCount == 2);
    CHECK(layout.FontRuns[0].SourceFont == &primary);
    CHECK(layout.FontRuns[0].FirstGlyph == 0);
    CHECK(layout.FontRuns[0].GlyphCount == 1);
    CHECK(layout.FontRuns[1].SourceFont == &fallback);
    CHECK(layout.FontRuns[1].FirstGlyph == 1);
    CHECK(layout.FontRuns[1].GlyphCount == 2);
}

TEST_CASE("Text layout keeps grapheme clusters on one wrapped line", "[Text][Layout][Unicode]")
{
    TextComponent component = MakeUnitTextComponent();
    component.LayoutSize = { 1.0f, 0.0f };
    component.WrapMode = TextWrapMode::Character;
    TextLayoutScratch scratch;
    TextLayout::DecodeUTF8("e\xcc\x81x", scratch);
    for (size_t index = 0; index < scratch.Tokens.Size(); index++)
    {
        scratch.Tokens[index].Advance = 1.0;
        scratch.Tokens[index].Renderable = true;
    }

    const TextLayoutResult layout = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);

    REQUIRE(layout.LineCount == 2);
    CHECK(layout.Lines[0].TokenStart == 0);
    CHECK(layout.Lines[0].TokenEnd == 2);
    CHECK(layout.Lines[0].CaretCount == 2);
    CHECK(layout.Carets[0].SourceByteOffset == 0);
    CHECK(layout.Carets[1].SourceByteOffset == 3);
    CHECK(layout.Lines[1].TokenStart == 2);
}

TEST_CASE("Text layout tabs advance to configurable tab stops", "[Text][Layout]")
{
    TextComponent component = MakeUnitTextComponent();
    component.Wrapping = false;
    TextLayoutScratch scratch;
    AddLayoutToken(scratch, U'a');
    AddLayoutToken(scratch, U'\t', true);
    AddLayoutToken(scratch, U'b');

    TextLayoutFontData fontData = UNIT_LAYOUT_FONT;
    fontData.TabWidth = 4;
    const TextLayoutResult fourSpaces = TextLayout::BuildPrepared(component, fontData, scratch);
    REQUIRE(fourSpaces.GlyphCount == 2);
    CHECK_THAT(fourSpaces.Glyphs[1].PenPosition.x, Catch::Matchers::WithinAbs(4.0f, 0.0001f));

    fontData.TabWidth = 8;
    const TextLayoutResult eightSpaces = TextLayout::BuildPrepared(component, fontData, scratch);
    REQUIRE(eightSpaces.GlyphCount == 2);
    CHECK_THAT(eightSpaces.Glyphs[1].PenPosition.x, Catch::Matchers::WithinAbs(8.0f, 0.0001f));
}

TEST_CASE("Text layout reports line and paragraph metrics", "[Text][Layout]")
{
    TextComponent component = MakeUnitTextComponent();
    component.LineSpacing = 0.25f;
    component.ParagraphSpacing = 0.5f;
    TextLayoutScratch scratch;
    AddLayoutToken(scratch, U'a');
    AddLayoutToken(scratch, U'\n', false, true);
    AddLayoutToken(scratch, U'b');

    const TextLayoutResult layout = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);

    REQUIRE(layout.LineCount == 2);
    CHECK_THAT(layout.LineAdvance, Catch::Matchers::WithinAbs(1.25f, 0.0001f));
    CHECK_THAT(layout.Lines[0].Baseline, Catch::Matchers::WithinAbs(0.0f, 0.0001f));
    CHECK_THAT(layout.Lines[1].Baseline, Catch::Matchers::WithinAbs(-1.75f, 0.0001f));
    CHECK_THAT(layout.Size.y, Catch::Matchers::WithinAbs(2.75f, 0.0001f));
}

TEST_CASE("Text layout hit testing follows visible caret geometry", "[Text][Layout]")
{
    TextComponent component = MakeUnitTextComponent();
    component.LayoutSize = { 2.0f, 0.0f };
    component.WrapMode = TextWrapMode::Character;
    TextLayoutScratch scratch;
    AddLayoutToken(scratch, U'\u00e9', false, false, 2);
    AddLayoutToken(scratch, U'b');
    AddLayoutToken(scratch, U'\U0001f642', false, false, 4);

    const TextLayoutResult wrapped = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);
    REQUIRE(wrapped.LineCount == 2);
    REQUIRE(wrapped.CaretCount == 5);

    const TextHitTestResult firstLineEnd = TextLayout::HitTest(wrapped, { 1.8f, 0.0f });
    REQUIRE(firstLineEnd.Valid);
    CHECK(firstLineEnd.LineIndex == 0);
    CHECK(firstLineEnd.SourceByteOffset == 3);
    CHECK(firstLineEnd.CaretPosition == glm::vec2(2.0f, 0.0f));

    const TextHitTestResult secondLineEnd = TextLayout::HitTest(wrapped, { 5.0f, -1.0f });
    REQUIRE(secondLineEnd.Valid);
    CHECK(secondLineEnd.LineIndex == 1);
    CHECK(secondLineEnd.SourceByteOffset == 7);
    CHECK(secondLineEnd.CaretPosition == glm::vec2(1.0f, -1.0f));
}

TEST_CASE("Text layout hit testing includes justification and ellipsis", "[Text][Layout]")
{
    TextComponent component = MakeUnitTextComponent();
    component.LayoutSize = { 10.0f, 10.0f };
    component.Wrapping = false;
    component.HorizontalAlignment = TextHorizontalAlignment::Flush;
    TextLayoutScratch scratch;
    AddLayoutToken(scratch, U'a');
    AddLayoutToken(scratch, U' ', true);
    AddLayoutToken(scratch, U'b');

    const TextLayoutResult justified = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);
    REQUIRE(justified.CaretCount == 4);
    const TextHitTestResult expandedSpace = TextLayout::HitTest(justified, { 8.5f, 0.0f });
    REQUIRE(expandedSpace.Valid);
    CHECK(expandedSpace.SourceByteOffset == 2);
    CHECK(expandedSpace.CaretPosition == glm::vec2(9.0f, 0.0f));

    component.HorizontalAlignment = TextHorizontalAlignment::Left;
    component.LayoutSize = { 10.0f, 1.0f };
    component.Overflow = TextOverflow::Ellipsis;
    TextLayoutScratch ellipsisScratch;
    AddLayoutToken(ellipsisScratch, U'a');
    AddLayoutToken(ellipsisScratch, U'\n', false, true);
    AddLayoutToken(ellipsisScratch, U'b');

    const TextLayoutResult ellipsized = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, ellipsisScratch);
    REQUIRE(ellipsized.LineCount == 1);
    REQUIRE(ellipsized.CaretCount == 3);
    const TextHitTestResult afterEllipsis = TextLayout::HitTest(ellipsized, { 2.0f, 0.0f });
    REQUIRE(afterEllipsis.Valid);
    CHECK(afterEllipsis.SourceByteOffset == 1);
    CHECK(afterEllipsis.CaretPosition == glm::vec2(2.0f, 0.0f));

    CHECK_FALSE(TextLayout::HitTest({}, { 0.0f, 0.0f }).Valid);
}

TEST_CASE("Text layout caret stops do not split combining sequences", "[Text][Layout]")
{
    TextComponent component = MakeUnitTextComponent();
    component.Wrapping = false;
    TextLayoutScratch scratch;
    AddLayoutToken(scratch, U'a');
    AddLayoutToken(scratch, U'\u0301', false, false, 2);
    scratch.Tokens[1].CombiningMark = true;

    const TextLayoutResult layout = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);
    REQUIRE(layout.CaretCount == 2);
    CHECK(layout.Carets[0].SourceByteOffset == 0);
    CHECK(layout.Carets[1].SourceByteOffset == 3);
}

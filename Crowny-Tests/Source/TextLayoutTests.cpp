#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Crowny/Ecs/Components.h"
#include "Crowny/Renderer/TextLayout.h"

using namespace Crowny;

namespace
{
    constexpr TextLayoutFontData UNIT_LAYOUT_FONT{ 0.8, -0.2, 1.0, 1.0, 1.0, 4, nullptr };

    void AddLayoutToken(TextLayoutScratch& scratch, char32_t codePoint, bool whitespace = false, bool newLine = false,
                        size_t sourceByteCount = 1)
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

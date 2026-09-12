#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/TextLayoutCache.h"

using namespace Crowny;

TEST_CASE("Owned text layouts survive source scratch reuse", "[Renderer][2D][Text]")
{
    Vector<TextLayoutGlyph> glyphs(2);
    glyphs[0].CodePoint = U'A';
    glyphs[1].CodePoint = U'B';
    Vector<TextLayoutCaret> carets(1);
    carets[0].SourceByteOffset = 5;
    TextLayoutResult result;
    result.Glyphs = glyphs.data();
    result.GlyphCount = glyphs.size();
    result.Carets = carets.data();
    result.CaretCount = carets.size();
    result.Size = { 17, 18 };
    OwnedTextLayout owned(result, {});
    glyphs.clear();
    carets.clear();
    glyphs.resize(100);
    const auto view = owned.View();
    REQUIRE(view.GlyphCount == 2);
    CHECK(view.Glyphs[0].CodePoint == U'A');
    CHECK(view.Glyphs[1].CodePoint == U'B');
    CHECK(view.Carets[0].SourceByteOffset == 5);
    CHECK(view.Size == glm::vec2(17, 18));
}

TEST_CASE("Text paint changes do not invalidate layout geometry", "[Renderer][2D][Text]")
{
    TextComponent first;
    first.Text = "Cached label";
    TextComponent painted = first;
    painted.Color = { 0.1f, 0.2f, 0.3f, 0.4f };
    painted.OutlineColor = { 1, 0, 0, 1 };
    painted.SortingLayer = 10;
    painted.ShadowOffset = { 2, 3 };
    CHECK(TextLayoutCache::SameLayout(first, painted));
    painted.LayoutSize = { 10, 20 };
    CHECK_FALSE(TextLayoutCache::SameLayout(first, painted));
    painted = first;
    painted.Text += "!";
    CHECK_FALSE(TextLayoutCache::SameLayout(first, painted));
}

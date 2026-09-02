#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Panels/ConsoleSplitLayout.h"

using namespace Crowny;

TEST_CASE("Console uses its full height when no message is selected", "[Editor][Console][Layout]")
{
    for (const float availableHeight : { -20.0f, 0.0f, 24.0f, 350.0f })
    {
        const ConsoleSplitLayout layout = BuildConsoleSplitLayout(availableHeight, false, 120.0f);
        const float safeHeight = std::max(0.0f, availableHeight);

        CHECK_FALSE(layout.DetailsVisible);
        CHECK(layout.SplitterHeight == Catch::Approx(0.0f));
        CHECK(layout.MessageHeight == Catch::Approx(safeHeight));
        CHECK(layout.MinimumMessageHeight == Catch::Approx(safeHeight));
        CHECK(layout.MaximumMessageHeight == Catch::Approx(safeHeight));
    }
}

TEST_CASE("Console keeps selected message details practical and resizable", "[Editor][Console][Layout]")
{
    constexpr float availableHeight = 350.0f;

    const ConsoleSplitLayout initial = BuildConsoleSplitLayout(availableHeight, true, 0.0f);
    CHECK(initial.DetailsVisible);
    CHECK(initial.SplitterHeight == Catch::Approx(6.0f));
    CHECK(initial.MinimumMessageHeight == Catch::Approx(120.0f));
    CHECK(initial.MaximumMessageHeight == Catch::Approx(244.0f));
    CHECK(initial.MessageHeight == Catch::Approx(initial.MaximumMessageHeight));
    CHECK(availableHeight - initial.SplitterHeight - initial.MessageHeight == Catch::Approx(100.0f));

    const ConsoleSplitLayout dragged = BuildConsoleSplitLayout(availableHeight, true, 180.0f);
    CHECK(dragged.MessageHeight == Catch::Approx(180.0f));

    const ConsoleSplitLayout clamped = BuildConsoleSplitLayout(availableHeight, true, 300.0f);
    CHECK(clamped.MessageHeight == Catch::Approx(clamped.MaximumMessageHeight));
}

TEST_CASE("Console split layout stays nonnegative when constrained", "[Editor][Console][Layout]")
{
    for (const float availableHeight : { -20.0f, 0.0f, 1.0f, 2.9f, 3.0f, 8.0f, 40.0f })
    {
        const ConsoleSplitLayout layout = BuildConsoleSplitLayout(availableHeight, true, 20.0f);
        const float safeHeight = std::max(0.0f, availableHeight);

        CHECK(layout.MessageHeight >= 0.0f);
        CHECK(layout.SplitterHeight >= 0.0f);
        CHECK(layout.MinimumMessageHeight >= 0.0f);
        CHECK(layout.MaximumMessageHeight >= layout.MinimumMessageHeight);
        CHECK(layout.MessageHeight <= safeHeight);
        CHECK(layout.MessageHeight + layout.SplitterHeight <= safeHeight);
    }
}

TEST_CASE("Console ignores remembered details height while no message is selected", "[Editor][Console][Layout]")
{
    constexpr float availableHeight = 350.0f;
    constexpr float rememberedMessageHeight = 180.0f;

    const ConsoleSplitLayout empty = BuildConsoleSplitLayout(availableHeight, false, rememberedMessageHeight);
    CHECK_FALSE(empty.DetailsVisible);
    CHECK(empty.MessageHeight == Catch::Approx(availableHeight));

    const ConsoleSplitLayout selected = BuildConsoleSplitLayout(availableHeight, true, rememberedMessageHeight);
    CHECK(selected.DetailsVisible);
    CHECK(selected.MessageHeight == Catch::Approx(rememberedMessageHeight));
}

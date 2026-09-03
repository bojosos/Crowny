#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Editor/ProjectHubLayout.h"

using namespace Crowny;
using Catch::Approx;

namespace
{
    void CheckLayoutBounds(const ProjectHubLayout& layout, float viewportWidth, float viewportHeight)
    {
        CHECK(layout.Card.X >= 0.0f);
        CHECK(layout.Card.Y >= 0.0f);
        CHECK(layout.Card.Width >= 0.0f);
        CHECK(layout.Card.Height >= 0.0f);
        CHECK(layout.Card.X + layout.Card.Width <= viewportWidth);
        CHECK(layout.Card.Y + layout.Card.Height <= viewportHeight);
        CHECK(layout.Card.X + layout.Card.Width * 0.5f == Approx(viewportWidth * 0.5f));
        CHECK(layout.Card.Y + layout.Card.Height * 0.5f == Approx(viewportHeight * 0.5f));

        CHECK(layout.Sidebar.X >= 0.0f);
        CHECK(layout.Sidebar.Y >= 0.0f);
        CHECK(layout.Sidebar.Width >= 0.0f);
        CHECK(layout.Sidebar.Height >= 0.0f);
        CHECK(layout.Sidebar.X + layout.Sidebar.Width <= layout.Card.Width);
        CHECK(layout.Sidebar.Y + layout.Sidebar.Height <= layout.Card.Height);

        CHECK(layout.Content.X >= 0.0f);
        CHECK(layout.Content.Y >= 0.0f);
        CHECK(layout.Content.Width >= 0.0f);
        CHECK(layout.Content.Height >= 0.0f);
        CHECK(layout.Content.X + layout.Content.Width <= layout.Card.Width);
        CHECK(layout.Content.Y + layout.Content.Height <= layout.Card.Height);
    }
} // namespace

TEST_CASE("Project Hub layout caps and centers a standard editor window", "[Editor][ProjectHub][Layout]")
{
    const ProjectHubLayout layout = CalculateProjectHubLayout(1280.0f, 720.0f);

    CheckLayoutBounds(layout, 1280.0f, 720.0f);
    CHECK(layout.Card.X == Approx(160.0f));
    CHECK(layout.Card.Y == Approx(40.0f));
    CHECK(layout.Card.Width == Approx(960.0f));
    CHECK(layout.Card.Height == Approx(640.0f));
}

TEST_CASE("Project Hub layout does not grow with a large viewport", "[Editor][ProjectHub][Layout]")
{
    const ProjectHubLayout layout = CalculateProjectHubLayout(1920.0f, 1080.0f);

    CheckLayoutBounds(layout, 1920.0f, 1080.0f);
    CHECK(layout.Card.X == Approx(480.0f));
    CHECK(layout.Card.Y == Approx(220.0f));
    CHECK(layout.Card.Width == Approx(960.0f));
    CHECK(layout.Card.Height == Approx(640.0f));
}

TEST_CASE("Project Hub layout remains usable in a constrained viewport", "[Editor][ProjectHub][Layout]")
{
    const ProjectHubLayout layout = CalculateProjectHubLayout(640.0f, 480.0f);

    CheckLayoutBounds(layout, 640.0f, 480.0f);
    CHECK(layout.Card.X == Approx(32.0f));
    CHECK(layout.Card.Y == Approx(32.0f));
    CHECK(layout.Card.Width == Approx(576.0f));
    CHECK(layout.Card.Height == Approx(416.0f));
    CHECK(layout.Sidebar.Width == Approx(168.0f));
    CHECK(layout.Content.Width > 0.0f);
}

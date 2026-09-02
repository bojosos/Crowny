#include <catch2/catch_test_macros.hpp>

#include "Panels/ConsoleSeverityToggleVisual.h"

#include <array>

using namespace Crowny;

namespace
{
    void CheckSeverityIdentity(const glm::vec4& expected, const glm::vec4& actual)
    {
        CHECK(actual.r == Catch::Approx(expected.r));
        CHECK(actual.g == Catch::Approx(expected.g));
        CHECK(actual.b == Catch::Approx(expected.b));
    }
} // namespace

TEST_CASE("Console severity toggles distinguish enabled and disabled filters", "[Editor][Console][UI]")
{
    const glm::vec4 severity{ 0.95f, 0.68f, 0.20f, 0.60f };
    const ConsoleSeverityToggleVisual enabled = BuildConsoleSeverityToggleVisual(severity, true);
    const ConsoleSeverityToggleVisual disabled = BuildConsoleSeverityToggleVisual(severity, false);

    CHECK(enabled.Text.a == Catch::Approx(severity.a));
    CHECK(disabled.Text.a > 0.0f);
    CHECK(disabled.Text.a < enabled.Text.a);

    CHECK(enabled.Fill.a > 0.0f);
    CHECK(enabled.HoveredFill.a > enabled.Fill.a);
    CHECK(enabled.ActiveFill.a > enabled.HoveredFill.a);
    CHECK(enabled.Border.a > enabled.Fill.a);

    CHECK(disabled.Fill.a == Catch::Approx(0.0f));
    CHECK(disabled.Border.a == Catch::Approx(0.0f));
    CHECK(disabled.HoveredFill.a > disabled.Fill.a);
    CHECK(disabled.ActiveFill.a > disabled.HoveredFill.a);

    CHECK(enabled.ActiveFill.a <= severity.a);
    CHECK(enabled.Border.a <= severity.a);
    CHECK(disabled.ActiveFill.a <= severity.a);
}

TEST_CASE("Console severity toggle visuals preserve each severity hue", "[Editor][Console][UI]")
{
    constexpr std::array<glm::vec4, 4> severities{
        glm::vec4{ 0.68f, 0.78f, 0.88f, 1.00f },
        glm::vec4{ 0.95f, 0.68f, 0.20f, 0.80f },
        glm::vec4{ 0.95f, 0.35f, 0.30f, 0.60f },
        glm::vec4{ 1.00f, 0.20f, 0.45f, 0.40f },
    };

    for (const glm::vec4& severity : severities)
    {
        for (const bool enabled : { false, true })
        {
            const ConsoleSeverityToggleVisual visual = BuildConsoleSeverityToggleVisual(severity, enabled);
            CheckSeverityIdentity(severity, visual.Text);
            CheckSeverityIdentity(severity, visual.Fill);
            CheckSeverityIdentity(severity, visual.HoveredFill);
            CheckSeverityIdentity(severity, visual.ActiveFill);
            CheckSeverityIdentity(severity, visual.Border);
        }
    }
}

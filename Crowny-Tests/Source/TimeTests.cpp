#include "Crowny/Common/Time.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace Crowny;

TEST_CASE("Time separates application frames from simulation time", "[Common][Time]")
{
    Time time;
    TimeSettings settings;
    settings.TimeScale = 0.5f;
    settings.MaxTimestep = 0.1f;
    settings.FixedTimestep = 1.0f / 50.0f;

    REQUIRE(time.GetFrameCount() == 0);
    REQUIRE(time.GetTime() == 0.0f);
    REQUIRE(time.GetRealtimeSinceStartup() == 0.0f);

    time.BeginFrame(0.2f);
    CHECK(time.GetFrameCount() == 1);
    CHECK(time.GetUnscaledDeltaTime() == Catch::Approx(0.2f));
    CHECK(time.GetRealtimeSinceStartup() == Catch::Approx(0.2f));
    CHECK(time.GetDeltaTime() == 0.0f);

    time.AdvanceSimulation(settings);
    CHECK(time.GetDeltaTime() == Catch::Approx(0.05f));
    CHECK(time.GetTime() == Catch::Approx(0.05f));
    CHECK(time.GetSmoothDeltaTime() == Catch::Approx(0.05f));
    CHECK(time.GetFixedDeltaTime() == Catch::Approx(1.0f / 50.0f));

    SECTION("simulation advances at most once per application frame")
    {
        time.AdvanceSimulation(settings);
        CHECK(time.GetTime() == Catch::Approx(0.05f));
    }

    SECTION("smooth delta time averages recent simulation frames")
    {
        time.BeginFrame(0.04f);
        time.AdvanceSimulation(settings);

        CHECK(time.GetFrameCount() == 2);
        CHECK(time.GetDeltaTime() == Catch::Approx(0.02f));
        CHECK(time.GetSmoothDeltaTime() == Catch::Approx(0.035f));
        CHECK(time.GetRealtimeSinceStartup() == Catch::Approx(0.24f));
    }

    SECTION("resetting simulation preserves application time")
    {
        time.ResetSimulation();

        CHECK(time.GetFrameCount() == 1);
        CHECK(time.GetRealtimeSinceStartup() == Catch::Approx(0.2f));
        CHECK(time.GetTime() == 0.0f);
        CHECK(time.GetDeltaTime() == 0.0f);
        CHECK(time.GetSmoothDeltaTime() == 0.0f);
    }
}

TEST_CASE("Time rejects invalid frame and settings values", "[Common][Time]")
{
    Time time;
    TimeSettings settings;
    settings.TimeScale = -1.0f;
    settings.MaxTimestep = -1.0f;
    settings.FixedTimestep = -1.0f;

    time.BeginFrame(-1.0f);
    time.AdvanceSimulation(settings);

    CHECK(time.GetUnscaledDeltaTime() == 0.0f);
    CHECK(time.GetDeltaTime() == 0.0f);
    CHECK(time.GetTime() == 0.0f);
    CHECK(time.GetRealtimeSinceStartup() == 0.0f);
    CHECK(time.GetFixedDeltaTime() == Catch::Approx(0.02f));
}

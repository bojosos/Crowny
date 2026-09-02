#include "Crowny/Common/Time.h"
#include "Crowny/Serialization/SettingsSerializer.h"

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
    CHECK(time.GetDeltaTime() == Catch::Approx(0.1f));
    CHECK(time.GetTime() == Catch::Approx(0.1f));
    CHECK(time.GetSmoothDeltaTime() == Catch::Approx(0.1f));
    CHECK(time.GetFixedDeltaTime() == Catch::Approx(1.0f / 50.0f));

    SECTION("simulation advances at most once per application frame")
    {
        const SimulationFrame repeated = time.AdvanceSimulation(settings);
        CHECK(time.GetTime() == Catch::Approx(0.1f));
        CHECK(repeated.FrameDelta.GetSeconds() == 0.0f);
        CHECK(repeated.FixedStepCount == 0);
    }

    SECTION("smooth delta time averages recent simulation frames")
    {
        time.BeginFrame(0.04f);
        time.AdvanceSimulation(settings);

        CHECK(time.GetFrameCount() == 2);
        CHECK(time.GetDeltaTime() == Catch::Approx(0.02f));
        CHECK(time.GetSmoothDeltaTime() == Catch::Approx(0.06f));
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

TEST_CASE("Time plans exact fixed simulation ticks", "[Common][Time][FixedUpdate]")
{
    Time time;
    TimeSettings settings;
    settings.FixedTimestep = 0.02f;
    settings.MaxTimestep = 0.25f;
    settings.MaxFixedStepsPerFrame = 8;

    time.BeginFrame(0.01f);
    SimulationFrame frame = time.AdvanceSimulation(settings);
    CHECK(frame.FrameDelta.GetSeconds() == Catch::Approx(0.01f));
    CHECK(frame.FixedDelta.GetSeconds() == Catch::Approx(0.02f));
    CHECK(frame.FixedStepCount == 0);
    CHECK(frame.FixedRemainder == Catch::Approx(0.01f));
    CHECK(frame.InterpolationAlpha == Catch::Approx(0.5f));

    time.BeginFrame(0.01f);
    frame = time.AdvanceSimulation(settings);
    CHECK(frame.FixedStepCount == 1);
    CHECK(frame.FixedRemainder == Catch::Approx(0.0f).margin(0.000001f));
    CHECK(frame.InterpolationAlpha == Catch::Approx(0.0f).margin(0.000001f));

    time.BeginFrame(0.055f);
    frame = time.AdvanceSimulation(settings);
    CHECK(frame.FixedStepCount == 2);
    CHECK(frame.FixedRemainder == Catch::Approx(0.015f));
    CHECK(frame.InterpolationAlpha == Catch::Approx(0.75f));
}

TEST_CASE("Time applies scaling and hitch limits once", "[Common][Time][FixedUpdate]")
{
    Time time;
    TimeSettings settings;
    settings.TimeScale = 2.0f;
    settings.MaxTimestep = 0.1f;
    settings.FixedTimestep = 0.02f;
    settings.MaxFixedStepsPerFrame = 8;

    time.BeginFrame(0.2f);
    const SimulationFrame frame = time.AdvanceSimulation(settings);

    CHECK(frame.FrameDelta.GetSeconds() == Catch::Approx(0.1f));
    CHECK(frame.FixedStepCount == 5);
    CHECK(frame.DroppedTime == Catch::Approx(0.3f));
    CHECK(time.GetTime() == Catch::Approx(0.1f));
}

TEST_CASE("Time exposes callback-local delta without changing frame time", "[Common][Time]")
{
    Time time;
    TimeSettings settings;
    time.BeginFrame(0.016f);
    time.AdvanceSimulation(settings);

    CHECK(time.GetDeltaTime() == Catch::Approx(0.016f));
    {
        Time::CallbackScope fixedUpdate(time, 0.02f);
        CHECK(time.GetDeltaTime() == Catch::Approx(0.02f));
        {
            Time::CallbackScope nestedCallback(time, 0.005f);
            CHECK(time.GetDeltaTime() == Catch::Approx(0.005f));
        }
        CHECK(time.GetDeltaTime() == Catch::Approx(0.02f));
    }
    CHECK(time.GetDeltaTime() == Catch::Approx(0.016f));
}

TEST_CASE("Time drops excess fixed backlog after the configured step budget", "[Common][Time][FixedUpdate]")
{
    Time time;
    TimeSettings settings;
    settings.MaxTimestep = 0.5f;
    settings.FixedTimestep = 0.02f;
    settings.MaxFixedStepsPerFrame = 3;

    time.BeginFrame(0.11f);
    const SimulationFrame frame = time.AdvanceSimulation(settings);

    CHECK(frame.FixedStepCount == 3);
    CHECK(frame.FrameDelta.GetSeconds() == Catch::Approx(0.06f));
    CHECK(frame.DroppedTime == Catch::Approx(0.05f));
    CHECK(frame.FixedRemainder == Catch::Approx(0.0f).margin(0.000001f));
    CHECK(frame.InterpolationAlpha == Catch::Approx(0.0f).margin(0.000001f));
}

TEST_CASE("Time preserves the fixed remainder when limiting a catch-up frame", "[Common][Time][FixedUpdate]")
{
    Time time;
    TimeSettings settings;
    settings.MaxTimestep = 0.5f;
    settings.FixedTimestep = 0.02f;
    settings.MaxFixedStepsPerFrame = 3;

    time.BeginFrame(0.019f);
    SimulationFrame frame = time.AdvanceSimulation(settings);
    REQUIRE(frame.FixedStepCount == 0);
    REQUIRE(frame.FixedRemainder == Catch::Approx(0.019f));

    time.BeginFrame(0.1f);
    frame = time.AdvanceSimulation(settings);

    CHECK(frame.FrameDelta.GetSeconds() == Catch::Approx(0.06f));
    CHECK(frame.FixedStepCount == 3);
    CHECK(frame.DroppedTime == Catch::Approx(0.04f));
    CHECK(frame.FixedRemainder == Catch::Approx(0.019f));
    CHECK(frame.InterpolationAlpha == Catch::Approx(0.95f));
    CHECK(time.GetTime() == Catch::Approx(0.079f));
}

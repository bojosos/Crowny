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

TEST_CASE("Time applies scaling and catch-up limits once", "[Common][Time][FixedUpdate]")
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

TEST_CASE("Time limits catch-up without discarding the fixed remainder", "[Common][Time][FixedUpdate]")
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

TEST_CASE("Simulation frames run fixed ticks before one variable update", "[Common][Time][FixedUpdate]")
{
    Time time;
    TimeSettings settings;
    settings.FixedTimestep = 0.02f;
    settings.MaxFixedStepsPerFrame = 8;
    time.BeginFrame(0.055f);
    const SimulationFrame frame = time.AdvanceSimulation(settings);

    Vector<String> phases;
    time.ExecuteSimulationFrame(
      frame,
      [&](Timestep timestep) {
          phases.push_back("fixed");
          CHECK(timestep.GetSeconds() == Catch::Approx(0.02f));
          CHECK(time.GetDeltaTime() == Catch::Approx(0.02f));
      },
      [&](Timestep timestep) {
          phases.push_back("variable");
          CHECK(timestep.GetSeconds() == Catch::Approx(0.055f));
          CHECK(time.GetDeltaTime() == Catch::Approx(0.055f));
      },
      [&](float alpha, Timestep remainder) {
          phases.push_back("prepare-render");
          CHECK(alpha == Catch::Approx(0.75f));
          CHECK(remainder.GetSeconds() == Catch::Approx(0.015f));
      });

    CHECK((phases == Vector<String>{ "fixed", "fixed", "variable", "prepare-render" }));
}

TEST_CASE("Simulation frame execution handles zero, one, multiple, and capped fixed ticks", "[Common][Time][FixedUpdate]")
{
    const auto checkFrame = [](float frameDelta, uint32_t maxFixedSteps, uint32_t expectedFixedSteps) {
        Time time;
        TimeSettings settings;
        settings.FixedTimestep = 0.02f;
        settings.MaxTimestep = 0.5f;
        settings.MaxFixedStepsPerFrame = maxFixedSteps;
        time.BeginFrame(frameDelta);
        const SimulationFrame frame = time.AdvanceSimulation(settings);

        uint32_t fixedUpdates = 0;
        uint32_t variableUpdates = 0;
        uint32_t renderPreparations = 0;
        time.ExecuteSimulationFrame(
          frame, [&](Timestep) { ++fixedUpdates; }, [&](Timestep) { ++variableUpdates; },
          [&](float, Timestep) { ++renderPreparations; });

        CHECK(fixedUpdates == expectedFixedSteps);
        CHECK(variableUpdates == 1);
        CHECK(renderPreparations == 1);
    };

    checkFrame(0.01f, 8, 0);
    checkFrame(0.02f, 8, 1);
    checkFrame(0.055f, 8, 2);
    checkFrame(0.11f, 3, 3);
}

TEST_CASE("Paused application frames do not enter the simulation accumulator", "[Common][Time][FixedUpdate]")
{
    Time time;
    TimeSettings settings;
    settings.FixedTimestep = 0.02f;

    time.BeginFrame(0.015f);
    REQUIRE(time.AdvanceSimulation(settings).FixedStepCount == 0);

    time.BeginFrame(0.5f);
    CHECK(time.GetDeltaTime() == 0.0f);

    time.BeginFrame(0.01f);
    const SimulationFrame resumed = time.AdvanceSimulation(settings);
    CHECK(resumed.FrameDelta.GetSeconds() == Catch::Approx(0.01f));
    CHECK(resumed.FixedStepCount == 1);
    CHECK(resumed.FixedRemainder == Catch::Approx(0.005f));

    time.ResetSimulation();
    time.BeginFrame(0.01f);
    const SimulationFrame restarted = time.AdvanceSimulation(settings);
    CHECK(restarted.FixedStepCount == 0);
    CHECK(restarted.FixedRemainder == Catch::Approx(0.01f));
}

TEST_CASE("Time settings preserve the fixed-step frame budget", "[Common][Time][Serialization]")
{
    Ref<TimeSettings> settings = CreateRef<TimeSettings>();
    settings->MaxFixedStepsPerFrame = 5;

    YAML::Emitter emitter;
    emitter << YAML::BeginMap;
    TimeSettingsSerializer::Serialize(settings, emitter);
    emitter << YAML::EndMap;

    const Ref<TimeSettings> restored = TimeSettingsSerializer::Deserialize(YAML::Load(emitter.c_str()));
    CHECK(restored->MaxFixedStepsPerFrame == 5);
}

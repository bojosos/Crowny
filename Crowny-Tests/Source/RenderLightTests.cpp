#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/RenderLight.h"

#include <glm/gtc/constants.hpp>

using namespace Crowny;

TEST_CASE("Render lights convert physical units into shader intensity", "[Renderer][Lights]")
{
    RenderLightDesc point;
    point.Type = LightType::Point;
    point.Intensity = 4.0f * glm::pi<float>() * 100.0f;
    const RenderLightData pointData = RenderLightWorld::BuildLightData(point);
    CHECK(pointData.ColorIntensity.w == Catch::Approx(100.0f));

    RenderLightDesc directional;
    directional.Type = LightType::Directional;
    directional.Intensity = 120000.0f;
    const RenderLightData directionalData = RenderLightWorld::BuildLightData(directional);
    CHECK(directionalData.ColorIntensity.w == Catch::Approx(120000.0f));
    CHECK(directionalData.PositionRange.w == 0.0f);

    RenderLightDesc spot;
    spot.Type = LightType::Spot;
    spot.Intensity = 1000.0f;
    spot.SpotOuterAngle = glm::radians(60.0f);
    const RenderLightData spotData = RenderLightWorld::BuildLightData(spot);
    const float solidAngle = 2.0f * glm::pi<float>() * (1.0f - std::cos(glm::radians(30.0f)));
    CHECK(spotData.ColorIntensity.w == Catch::Approx(1000.0f / solidAngle));
}

TEST_CASE("Render light changes coalesce and handles are generational", "[Renderer][Lights]")
{
    RenderLightWorld world(2);
    RenderLightDesc desc;
    const RenderLightHandle first = world.CreateLight(desc);
    REQUIRE(first.IsValid());

    desc.Intensity = 2500.0f;
    REQUIRE(world.UpdateLight(first, desc));
    Vector<RenderLightChange> changes;
    world.DrainChanges(changes);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].Type == RenderLightChangeType::Create);
    CHECK(changes[0].Data.ColorIntensity.w == Catch::Approx(2500.0f / (4.0f * glm::pi<float>())));

    REQUIRE(world.DestroyLight(first));
    world.DrainChanges(changes);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].Type == RenderLightChangeType::Destroy);
    CHECK_FALSE(world.IsAlive(first));

    const RenderLightHandle second = world.CreateLight(desc);
    REQUIRE(second.IsValid());
    CHECK(second.GetIndex() == first.GetIndex());
    CHECK(second.GetGeneration() != first.GetGeneration());
    CHECK_FALSE(world.UpdateLight(first, desc));
}

TEST_CASE("Color temperature conversion is bounded and physically ordered", "[Renderer][Lights]")
{
    const glm::vec3 warm = RenderLightWorld::ColorTemperatureToLinearRgb(1800.0f);
    const glm::vec3 daylight = RenderLightWorld::ColorTemperatureToLinearRgb(6500.0f);
    CHECK(warm.r > warm.b);
    CHECK(daylight.b > warm.b);
    CHECK(glm::all(glm::greaterThanEqual(warm, glm::vec3(0.0f))));
    CHECK(glm::all(glm::lessThanEqual(daylight, glm::vec3(1.0f))));
}

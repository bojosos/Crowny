#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/LocalShadowViews.h"

using namespace Crowny;

namespace
{
    bool IsFinite(const glm::mat4& value)
    {
        for (uint32_t column = 0; column < 4; column++)
            for (uint32_t row = 0; row < 4; row++)
                if (!std::isfinite(value[column][row]))
                    return false;
        return true;
    }
}

TEST_CASE("Spot shadow view follows the physical light cone", "[Renderer][Lights][Shadows]")
{
    RenderLightDesc desc;
    desc.Type = LightType::Spot;
    desc.Position = { 2.0f, 3.0f, 4.0f };
    desc.Direction = { 0.0f, -1.0f, 0.0f };
    desc.Range = 25.0f;
    desc.SpotOuterAngle = glm::radians(50.0f);
    desc.Shadows.NearPlane = 0.2f;
    const RenderLightData light = RenderLightWorld::BuildLightData(desc);

    const LocalShadowView view = LocalShadowViewBuilder::BuildSpot(light, desc.Shadows);
    CHECK(view.NearPlane == Catch::Approx(0.2f));
    CHECK(view.FarPlane == Catch::Approx(25.0f));
    CHECK(view.Direction == glm::vec3(0.0f, -1.0f, 0.0f));
    CHECK(IsFinite(view.ViewProjection));
    const glm::vec4 lightInView = view.View * glm::vec4(desc.Position, 1.0f);
    CHECK(glm::length(glm::vec3(lightInView)) == Catch::Approx(0.0f).margin(0.0001f));
}

TEST_CASE("Point lights produce six finite cube shadow views", "[Renderer][Lights][Shadows]")
{
    RenderLightDesc desc;
    desc.Type = LightType::Point;
    desc.Position = { -3.0f, 1.0f, 8.0f };
    desc.Range = 40.0f;
    desc.Shadows.NearPlane = 0.1f;
    const RenderLightData light = RenderLightWorld::BuildLightData(desc);

    std::array<LocalShadowView, 6> views;
    LocalShadowViewBuilder::BuildPoint(light, desc.Shadows, views);
    for (uint32_t face = 0; face < views.size(); face++)
    {
        CHECK(views[face].Face == face);
        CHECK(views[face].NearPlane == Catch::Approx(0.1f));
        CHECK(views[face].FarPlane == Catch::Approx(40.0f));
        CHECK(IsFinite(views[face].ViewProjection));
    }
    CHECK(views[0].Direction == -views[1].Direction);
    CHECK(views[2].Direction == -views[3].Direction);
    CHECK(views[4].Direction == -views[5].Direction);
}

#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/ShadowGpuData.h"

using namespace Crowny;

TEST_CASE("GPU spot shadow data maps its atlas tile", "[Renderer][Shadows]")
{
    LocalShadowView view;
    view.ViewProjection = glm::mat4(2.0f);
    view.NearPlane = 0.1f;
    view.FarPlane = 50.0f;
    ShadowAtlasAllocation allocation;
    allocation.Light = RenderLightHandle::FromParts(1, 1);
    allocation.X = 512;
    allocation.Y = 256;
    allocation.Size = 512;
    LightShadowSettings settings;
    settings.Mode = LightShadowMode::Soft;
    settings.Bias = 0.002f;
    settings.NormalBias = 0.03f;

    const GpuShadowViewData result = ShadowGpuDataBuilder::BuildSpot(view, allocation, 2048, settings);
    CHECK(result.AtlasScaleBias == glm::vec4(0.25f, 0.25f, 0.25f, 0.125f));
    CHECK(result.SplitDepthBias == glm::vec4(0.1f, 50.0f, 0.002f, 0.03f));
    const GpuShadowLightData light = ShadowGpuDataBuilder::BuildLightRecord(4, 1, LightType::Spot, settings);
    CHECK(light.ViewOffset == 4);
    CHECK(light.ViewCount == 1);
    CHECK((light.Flags & static_cast<uint32_t>(GpuShadowFlags::Valid)) != 0);
    CHECK((light.Flags & static_cast<uint32_t>(GpuShadowFlags::Soft)) != 0);
}

TEST_CASE("GPU point shadow data keeps six cube faces", "[Renderer][Shadows]")
{
    std::array<LocalShadowView, 6> views;
    for (uint32_t face = 0; face < views.size(); face++)
    {
        views[face].Face = face;
        views[face].FarPlane = 25.0f;
    }
    LightShadowSettings settings;
    settings.Mode = LightShadowMode::Hard;
    Vector<GpuShadowViewData> output;
    ShadowGpuDataBuilder::BuildPoint(views, 3, settings, output);
    REQUIRE(output.size() == 6);
    for (uint32_t face = 0; face < output.size(); face++)
    {
        CHECK(output[face].Metadata.x == 3);
        CHECK(output[face].Metadata.y == face);
    }
    const GpuShadowLightData light = ShadowGpuDataBuilder::BuildLightRecord(0, 6, LightType::Point, settings);
    CHECK((light.Flags & static_cast<uint32_t>(GpuShadowFlags::Cube)) != 0);
}

TEST_CASE("GPU directional shadows preserve cascade layers and splits", "[Renderer][Shadows]")
{
    std::array<DirectionalShadowCascade, 3> cascades;
    for (uint32_t index = 0; index < cascades.size(); index++)
    {
        cascades[index].NearSplit = static_cast<float>(index);
        cascades[index].FarSplit = static_cast<float>(index + 1u);
    }
    LightShadowSettings settings;
    settings.Mode = LightShadowMode::Soft;
    Vector<GpuShadowViewData> output;
    ShadowGpuDataBuilder::BuildDirectionalArray(cascades.data(), static_cast<uint32_t>(cascades.size()), settings, output);
    REQUIRE(output.size() == cascades.size());
    for (uint32_t index = 0; index < output.size(); index++)
    {
        CHECK(output[index].Metadata.x == index);
        CHECK(output[index].SplitDepthBias.x == static_cast<float>(index));
        CHECK(output[index].SplitDepthBias.y == static_cast<float>(index + 1u));
    }
}

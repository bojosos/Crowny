#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/Renderer/ClusteredLightGrid.h"

#include <glm/gtc/matrix_transform.hpp>

using namespace Crowny;

TEST_CASE("Clustered lights use logarithmic depth slices", "[Renderer][Lights][Clusters]")
{
    CHECK(ClusteredLightBuilder::DepthToSlice(0.1f, 0.1f, 1000.0f, 4) == 0);
    CHECK(ClusteredLightBuilder::DepthToSlice(1.0f, 0.1f, 1000.0f, 4) == 1);
    CHECK(ClusteredLightBuilder::DepthToSlice(10.0f, 0.1f, 1000.0f, 4) == 2);
    CHECK(ClusteredLightBuilder::DepthToSlice(1000.0f, 0.1f, 1000.0f, 4) == 3);
}

TEST_CASE("Clustered light runtime settings resolve once for allocation and dispatch", "[Renderer][Lights][Clusters]")
{
    RenderPipelineSettings settings;
    settings.ClusterTileSize = 7;
    settings.ClusterDepthSlices = 3;
    settings.MaxLightsPerCluster = 256;
    settings.MaxDirectionalLights = 2;

    const ClusteredLightGridDesc desc = ClusteredLightBuilder::ResolveDesc(settings, 17, 9);
    CHECK(desc.TileSize == 7);
    CHECK(desc.DepthSlices == 3);
    CHECK(desc.MaxLightsPerCluster == 128);
    CHECK(desc.MaxDirectionalLights == 2);
    CHECK(ClusteredLightBuilder::GetDimensions(desc) == glm::uvec3(3u, 2u, 3u));
    CHECK(ClusteredLightBuilder::GetClusterCount(desc) == 18u);

    settings.ClusterTileSize = 0;
    settings.ClusterDepthSlices = 0;
    settings.MaxLightsPerCluster = 0;
    settings.MaxDirectionalLights = 0;
    const ClusteredLightGridDesc minimum = ClusteredLightBuilder::ResolveDesc(settings, 0, 0);
    CHECK(minimum.TileSize == 1);
    CHECK(minimum.DepthSlices == 1);
    CHECK(minimum.MaxLightsPerCluster == 1);
    CHECK(minimum.MaxDirectionalLights == 1);
    CHECK(ClusteredLightBuilder::GetClusterCount(minimum) == 1u);
}

TEST_CASE("Clustered lights assign local lights and separate directionals", "[Renderer][Lights][Clusters]")
{
    ClusteredLightGridDesc desc;
    desc.ViewportWidth = 64;
    desc.ViewportHeight = 64;
    desc.TileSize = 16;
    desc.DepthSlices = 4;
    desc.NearPlane = 0.1f;
    desc.FarPlane = 100.0f;

    RenderLightDesc point;
    point.Position = { 0.0f, 0.0f, -5.0f };
    point.Range = 1.0f;
    RenderLightDesc sun;
    sun.Type = LightType::Directional;
    const std::array lights{ RenderLightWorld::BuildLightData(point), RenderLightWorld::BuildLightData(sun) };

    ClusteredLightGrid grid;
    ClusteredLightBuilder::Build(desc, glm::mat4(1.0f), glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f), lights.data(),
                                 static_cast<uint32_t>(lights.size()), grid);
    CHECK(grid.Dimensions == glm::uvec3(4u, 4u, 4u));
    CHECK_FALSE(grid.LightIndices.empty());
    REQUIRE(grid.DirectionalLightIndices.size() == 1);
    CHECK(grid.DirectionalLightIndices[0] == 1);
}

TEST_CASE("Clustered light overflow is explicit and bounded", "[Renderer][Lights][Clusters]")
{
    ClusteredLightGridDesc desc;
    desc.ViewportWidth = 16;
    desc.ViewportHeight = 16;
    desc.TileSize = 16;
    desc.DepthSlices = 1;
    desc.MaxLightsPerCluster = 2;
    desc.NearPlane = 0.1f;
    desc.FarPlane = 10.0f;

    RenderLightDesc light;
    light.Position = { 0.0f, 0.0f, -1.0f };
    light.Range = 10.0f;
    const std::array lights{ RenderLightWorld::BuildLightData(light), RenderLightWorld::BuildLightData(light),
                             RenderLightWorld::BuildLightData(light) };
    ClusteredLightGrid grid;
    ClusteredLightBuilder::Build(desc, glm::mat4(1.0f), glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 10.0f), lights.data(),
                                 static_cast<uint32_t>(lights.size()), grid);
    REQUIRE(grid.Cells.size() == 1);
    CHECK(grid.Cells[0].Count == 2);
    CHECK(grid.LightIndices.size() == 2);
    CHECK(grid.OverflowCount == 1);
}

TEST_CASE("Clustered light directional lists honor their configured limit", "[Renderer][Lights][Clusters]")
{
    ClusteredLightGridDesc desc;
    desc.ViewportWidth = 16;
    desc.ViewportHeight = 16;
    desc.DepthSlices = 1;
    desc.MaxDirectionalLights = 1;

    RenderLightDesc light;
    light.Type = LightType::Directional;
    const std::array lights{ RenderLightWorld::BuildLightData(light), RenderLightWorld::BuildLightData(light) };
    ClusteredLightGrid grid;
    ClusteredLightBuilder::Build(desc, glm::mat4(1.0f), glm::mat4(1.0f), lights.data(), static_cast<uint32_t>(lights.size()), grid);
    REQUIRE(grid.DirectionalLightIndices.size() == 1);
    CHECK(grid.DirectionalLightIndices[0] == 0);
    CHECK(grid.OverflowCount == 1);
}

TEST_CASE("Clustered light builds allocate nothing after warm-up", "[Renderer][Lights][Clusters][Memory][Frame]")
{
    constexpr std::array<uint32_t, 3> lightCounts{ 1u, 1000u, 10000u };
    constexpr uint32_t frameCount = 120u;

    ClusteredLightGridDesc desc;
    desc.ViewportWidth = 1;
    desc.ViewportHeight = 1;
    desc.TileSize = 1;
    desc.DepthSlices = 1;
    desc.MaxLightsPerCluster = 128;
    desc.NearPlane = 0.1f;
    desc.FarPlane = 10.0f;
    const glm::mat4 projection = glm::perspective(glm::radians(60.0f), 1.0f, desc.NearPlane, desc.FarPlane);

    RenderLightDesc light;
    light.Position = { 0.0f, 0.0f, -1.0f };
    light.Range = 0.25f;
    const RenderLightData packedLight = RenderLightWorld::BuildLightData(light);

    for (uint32_t lightCount : lightCounts)
    {
        Vector<RenderLightData> lights(lightCount, packedLight);
        ClusteredLightGrid grid;
        ClusteredLightBuilder::Build(desc, glm::mat4(1.0f), projection, lights.data(), lightCount, grid);
        ClusteredLightBuilder::Build(desc, glm::mat4(1.0f), projection, lights.data(), lightCount, grid);

        const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
        uint64_t checksum = 0;
        for (uint32_t frame = 0; frame < frameCount; frame++)
        {
            ClusteredLightBuilder::Build(desc, glm::mat4(1.0f), projection, lights.data(), lightCount, grid);
            checksum += grid.Cells[0].Count;
        }
        const Memory::ThreadAllocationSnapshot after = Memory::GetThreadAllocationSnapshot();
        const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, after);

        const uint32_t expectedCount = std::min(lightCount, desc.MaxLightsPerCluster);
        INFO("Light count: " << lightCount);
        CHECK(checksum == static_cast<uint64_t>(expectedCount) * frameCount);
        CHECK(grid.Cells.size() == 1u);
        CHECK(grid.LightIndices.size() == expectedCount);
        CHECK(grid.OverflowCount == lightCount - expectedCount);
        CHECK(delta.AllocationCount == 0u);
        CHECK(delta.RequestedBytes == 0u);
    }
}

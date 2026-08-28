#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/GpuScene.h"

using namespace Crowny;

TEST_CASE("GPU scene mirrors sparse instance and light changes", "[Renderer][GpuScene]")
{
    GpuScene scene(false);
    RenderWorld instances;
    RenderLightWorld lights;
    RenderInstanceDesc instanceDesc;
    instanceDesc.MeshHandle = 7;
    instanceDesc.RenderLayerOrder = 11;
    const RenderInstanceHandle instance = instances.CreateInstance(instanceDesc);
    RenderLightDesc lightDesc;
    const RenderLightHandle light = lights.CreateLight(lightDesc);

    Vector<RenderWorldChange> instanceChanges;
    Vector<RenderLightChange> lightChanges;
    instances.DrainChanges(instanceChanges);
    lights.DrainChanges(lightChanges);
    scene.Apply(instanceChanges.data(), static_cast<uint32_t>(instanceChanges.size()), lightChanges.data(),
                static_cast<uint32_t>(lightChanges.size()));

    RenderInstanceData instanceData;
    RenderLightData lightData;
    int32_t renderLayerOrder = 0;
    CHECK(scene.TryGetInstance(instance, instanceData, &renderLayerOrder));
    CHECK(RenderWorld::GetMeshHandle(instanceData.Draw) == 7);
    CHECK(renderLayerOrder == 11);
    CHECK(scene.TryGetLight(light, lightData));
    CHECK(scene.GetStats().ActiveInstances == 1);
    CHECK(scene.GetStats().ActiveLights == 1);
    CHECK(scene.GetStats().UploadedBytes == 0);

    REQUIRE(instances.DestroyInstance(instance));
    REQUIRE(lights.DestroyLight(light));
    instances.DrainChanges(instanceChanges);
    lights.DrainChanges(lightChanges);
    scene.Apply(instanceChanges.data(), static_cast<uint32_t>(instanceChanges.size()), lightChanges.data(),
                static_cast<uint32_t>(lightChanges.size()));
    CHECK_FALSE(scene.TryGetInstance(instance, instanceData));
    CHECK_FALSE(scene.TryGetLight(light, lightData));
    CHECK(scene.GetStats().ActiveInstances == 0);
    CHECK(scene.GetStats().ActiveLights == 0);
}

TEST_CASE("Stable GPU scenes produce no upload work", "[Renderer][GpuScene]")
{
    GpuScene scene(false);
    scene.Apply(nullptr, 0, nullptr, 0);
    CHECK(scene.GetStats().UploadedBytes == 0);
    CHECK(scene.GetStats().InstanceRanges == 0);
    CHECK(scene.GetStats().LightRanges == 0);
    CHECK(scene.GetStats().GeometryHeapPages == 0);
    CHECK(scene.GetStats().GeometryHeapCapacityBytes == 0);
    CHECK(scene.GetStats().GeometryUploadBytes == 0);
    CHECK_FALSE(scene.HasForwardOnlyOpaqueMaterials());

    scene.Reset();
    CHECK_FALSE(scene.HasForwardOnlyOpaqueMaterials());
}

TEST_CASE("GPU scene rejects stale and invalid sparse updates", "[Renderer][GpuScene]")
{
    GpuScene scene(false);
    RenderInstanceData initial;
    initial.Draw.MeshAndFlags = 7;
    const RenderInstanceHandle current = RenderInstanceHandle::FromParts(3, 2);
    const RenderInstanceHandle stale = RenderInstanceHandle::FromParts(3, 1);
    RenderWorldChange create{ current, RenderWorldChangeType::Create, RenderWorldDirtyFlags::All, initial };
    scene.Apply(&create, 1, nullptr, 0);

    RenderInstanceData replacement = initial;
    replacement.Draw.MeshAndFlags = 99;
    RenderWorldChange staleUpdate{ stale, RenderWorldChangeType::Update, RenderWorldDirtyFlags::All, replacement };
    RenderWorldChange invalidUpdate{ {}, RenderWorldChangeType::Update, RenderWorldDirtyFlags::All, replacement };
    const RenderWorldChange updates[] = { staleUpdate, invalidUpdate };
    scene.Apply(updates, 2, nullptr, 0);

    RenderInstanceData result;
    REQUIRE(scene.TryGetInstance(current, result));
    CHECK(result.Draw.MeshAndFlags == 7);
    CHECK(scene.GetStats().ActiveInstances == 1);
}

TEST_CASE("GPU scene retains packed shadow tables without a graphics device", "[Renderer][GpuScene][Shadows]")
{
    GpuScene scene(false);
    const GpuShadowLightData light{ 0, 1, static_cast<uint32_t>(LightType::Spot), static_cast<uint32_t>(GpuShadowFlags::Valid) };
    GpuShadowViewData view;
    view.AtlasScaleBias = { 0.5f, 0.5f, 0.25f, 0.25f };
    scene.UploadShadowData(&light, 1, &view, 1);
    CHECK(scene.GetShadowLightCount() == 1);
    CHECK(scene.GetShadowViewCount() == 1);
    CHECK(scene.GetStats().UploadedBytes == 0);

    scene.UploadShadowData(&light, 1, &view, 1);
    CHECK(scene.GetStats().ShadowRanges == 0);
}

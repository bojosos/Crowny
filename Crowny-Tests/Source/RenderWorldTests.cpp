#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/RenderWorld.h"

#include <glm/gtc/matrix_transform.hpp>

using namespace Crowny;

TEST_CASE("RenderWorld uses compact GPU instance records", "[Renderer][RenderWorld]")
{
    CHECK(sizeof(RenderTransformRecord) == 96);
    CHECK(sizeof(RenderCullingRecord) == 16);
    CHECK(sizeof(RenderDrawRecord) == 16);
    CHECK(sizeof(RenderInstanceData) == 128);
    CHECK(RenderInstanceHandle::MaxInstances == 1'048'576);
}

TEST_CASE("RenderWorld preserves forward-only LOD routing in packed instance flags", "[Renderer][RenderWorld][Materials]")
{
    RenderWorld world;
    RenderInstanceDesc desc;
    desc.Flags = RenderInstanceFlags::Visible | RenderInstanceFlags::ForceLod0;
    const RenderInstanceHandle handle = world.CreateInstance(desc);
    RenderInstanceData data;

    REQUIRE(world.TryGetInstance(handle, data));
    CHECK(HasFlag(RenderWorld::GetFlags(data.Draw), RenderInstanceFlags::Visible));
    CHECK(HasFlag(RenderWorld::GetFlags(data.Draw), RenderInstanceFlags::ForceLod0));
}

TEST_CASE("RenderWorld rejects stale generational handles", "[Renderer][RenderWorld]")
{
    RenderWorld world(1);
    Vector<RenderWorldChange> changes;

    const RenderInstanceHandle first = world.CreateInstance({});
    REQUIRE(first.IsValid());
    world.DrainChanges(changes);
    REQUIRE(world.DestroyInstance(first));
    world.DrainChanges(changes);

    const RenderInstanceHandle second = world.CreateInstance({});
    REQUIRE(second.IsValid());
    CHECK(second.GetIndex() == first.GetIndex());
    CHECK(second.GetGeneration() != first.GetGeneration());
    CHECK_FALSE(world.IsAlive(first));
    CHECK(world.IsAlive(second));
    CHECK_FALSE(world.UpdateTransform(first, glm::mat4(1.0f), glm::vec4(0.0f)));
}

TEST_CASE("RenderWorld coalesces changes before render-thread consumption", "[Renderer][RenderWorld]")
{
    RenderWorld world;
    RenderInstanceDesc desc;
    desc.MeshHandle = 17;
    desc.MaterialHandle = 23;
    desc.LodBias = -1.25f;
    desc.RenderLayerOrder = 12;

    const RenderInstanceHandle handle = world.CreateInstance(desc);
    REQUIRE(handle.IsValid());
    REQUIRE(world.UpdateTransform(handle, glm::translate(glm::mat4(1.0f), glm::vec3(4.0f, 5.0f, 6.0f)),
                                  glm::vec4(4.0f, 5.0f, 6.0f, 2.0f)));

    Vector<RenderWorldChange> changes;
    world.DrainChanges(changes);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].Type == RenderWorldChangeType::Create);
    CHECK(changes[0].DirtyFlags == RenderWorldDirtyFlags::All);
    CHECK(RenderWorld::GetMeshHandle(changes[0].Data.Draw) == 17);
    CHECK(RenderWorld::GetMaterialHandle(changes[0].Data.Draw) == 23);
    CHECK(RenderWorld::GetLodBias(changes[0].Data.Draw) == -1.25f);
    CHECK(changes[0].RenderLayerOrder == 12);
    CHECK(changes[0].Data.Culling.BoundingSphere.w == 2.0f);

    world.DrainChanges(changes);
    CHECK(changes.empty());
}

TEST_CASE("RenderWorld keeps render-layer ordering outside the fixed GPU instance record", "[Renderer][RenderWorld][Transparency]")
{
    RenderWorld world;
    RenderInstanceDesc desc;
    desc.RenderLayerOrder = 7;
    const RenderInstanceHandle handle = world.CreateInstance(desc);

    Vector<RenderWorldChange> changes;
    world.DrainChanges(changes);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].RenderLayerOrder == 7);

    RenderInstanceData data;
    int32_t renderLayerOrder = 0;
    REQUIRE(world.TryGetInstance(handle, data, &renderLayerOrder));
    CHECK(renderLayerOrder == 7);
    CHECK(sizeof(RenderInstanceData) == 128);

    desc.RenderLayerOrder = -4;
    REQUIRE(world.UpdateInstance(handle, desc));
    world.DrainChanges(changes);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].RenderLayerOrder == -4);
}

TEST_CASE("RenderWorld settles previous transforms one frame after movement", "[Renderer][RenderWorld][MotionVectors]")
{
    RenderWorld world;
    const RenderInstanceHandle handle = world.CreateInstance({});
    Vector<RenderWorldChange> changes;
    world.DrainChanges(changes);

    const glm::mat4 moved = glm::translate(glm::mat4(1.0f), glm::vec3(4.0f, 5.0f, 6.0f));
    REQUIRE(world.UpdateTransform(handle, moved, glm::vec4(4.0f, 5.0f, 6.0f, 2.0f)));
    world.DrainChanges(changes);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].Data.Transforms.Current.ToMatrix() == moved);
    CHECK(changes[0].Data.Transforms.Previous.ToMatrix() == glm::mat4(1.0f));

    world.DrainChanges(changes);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].DirtyFlags == RenderWorldDirtyFlags::Transform);
    CHECK(changes[0].Data.Transforms.Current.ToMatrix() == moved);
    CHECK(changes[0].Data.Transforms.Previous.ToMatrix() == moved);

    world.DrainChanges(changes);
    CHECK(changes.empty());
}

TEST_CASE("RenderWorld cancels instances destroyed before first upload", "[Renderer][RenderWorld]")
{
    RenderWorld world;
    const RenderInstanceHandle handle = world.CreateInstance({});
    REQUIRE(world.DestroyInstance(handle));

    Vector<RenderWorldChange> changes;
    world.DrainChanges(changes);
    CHECK(changes.empty());
    CHECK(world.GetActiveInstanceCount() == 0);
}

TEST_CASE("AffineTransform3x4 preserves affine matrices", "[Renderer][RenderWorld]")
{
    const glm::mat4 original = glm::translate(glm::mat4(1.0f), glm::vec3(7.0f, -3.0f, 2.0f)) *
                               glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 3.0f, 4.0f));
    const glm::mat4 restored = AffineTransform3x4::FromMatrix(original).ToMatrix();
    CHECK(restored == original);
}

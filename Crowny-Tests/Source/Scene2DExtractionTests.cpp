#include <catch2/catch_test_macros.hpp>

#include "Crowny/Ecs/Components.h"
#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/Renderer/Camera.h"
#include "Crowny/Renderer/GpuWorld2D.h"
#include "Crowny/Renderer/RenderSnapshot.h"
#include "Crowny/Scene/Scene.h"
#include "Crowny/Scene/SceneRenderer.h"

using namespace Crowny;

TEST_CASE("2D snapshots detect direct component writes and retain prior values", "[Renderer][2D][SceneSync]")
{
    const auto scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Persistent sprite");
    entity.AddComponent<SpriteRendererComponent>();
    SceneRenderer renderer(scene, nullptr);
    Camera camera;
    RenderSnapshot first;
    first.FrameNumber = 1;
    renderer.ExtractSnapshot(first, camera, glm::mat4(1), false);
    REQUIRE(first.SpriteHandles.Size() == 1);
    CHECK(first.Sprites.Empty());
    REQUIRE(first.RenderWorld2DChanges.Size() == 1);
    const auto handle = first.SpriteHandles[0];
    CHECK(first.RenderWorld2DChanges[0].Type == RenderChange2DType::Create);

    RenderSnapshot second;
    second.FrameNumber = 2;
    entity.GetComponent<SpriteRendererComponent>().Color = { 0.2f, 0.3f, 0.4f, 0.5f };
    renderer.ExtractSnapshot(second, camera, glm::mat4(1), false);
    REQUIRE(second.RenderWorld2DChanges.Size() == 1);
    CHECK(second.SpriteHandles[0] == handle);
    CHECK(second.RenderWorld2DChanges[0].Type == RenderChange2DType::Update);
    CHECK(second.RenderWorld2DChanges[0].Data.Color.a == 0.5f);
    CHECK(first.RenderWorld2DChanges[0].Data.Color.a == 1.0f);

    second.FrameNumber = 3;
    renderer.ExtractSnapshot(second, camera, glm::mat4(1), false);
    CHECK(second.RenderWorld2DChanges.Empty());
    scene->DestroyEntity(entity);
    second.FrameNumber = 4;
    renderer.ExtractSnapshot(second, camera, glm::mat4(1), false);
    REQUIRE(second.RenderWorld2DChanges.Size() == 1);
    CHECK(second.RenderWorld2DChanges[0].Type == RenderChange2DType::Destroy);
    CHECK(second.RenderWorld2DChanges[0].Handle == handle);
}

TEST_CASE("2D extraction shares handles across cameras and retires scene content", "[Renderer][2D][SceneSync]")
{
    const auto scene = CreateRef<Scene>(false);
    scene->CreateEntity("Sprite").AddComponent<SpriteRendererComponent>();
    SceneRenderer renderer(scene, nullptr);
    Camera firstCamera, secondCamera;
    RenderSnapshot first, second;
    first.FrameNumber = second.FrameNumber = 1;
    renderer.ExtractSnapshot(first, firstCamera, glm::mat4(1), false);
    renderer.ExtractSnapshot(second, secondCamera, glm::mat4(1), false);
    REQUIRE(first.SpriteHandles.Size() == 1);
    REQUIRE(second.SpriteHandles.Size() == 1);
    CHECK(first.SpriteHandles[0] == second.SpriteHandles[0]);
    CHECK(first.World2DLifetime == second.World2DLifetime);
    CHECK(first.HistoryNamespace != second.HistoryNamespace);
    CHECK(second.RenderWorld2DChanges.Empty());

    renderer.SetScene(CreateRef<Scene>(false));
    second.FrameNumber = 2;
    renderer.ExtractSnapshot(second, secondCamera, glm::mat4(1), false);
    REQUIRE(second.RenderWorld2DChanges.Size() == 1);
    CHECK(second.RenderWorld2DChanges[0].Type == RenderChange2DType::Destroy);
    CHECK(second.SpriteHandles.Empty());
    // The old queued snapshot still owns its data and owner lifetime token.
    CHECK(first.World2DLifetime != nullptr);
    CHECK(first.RenderWorld2DChanges[0].Type == RenderChange2DType::Create);
}

TEST_CASE("2D extraction distinguishes replaced components and recycled entity slots", "[Renderer][2D][SceneSync]")
{
    const auto scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Original sprite");
    entity.AddComponent<SpriteRendererComponent>();
    SceneRenderer renderer(scene, nullptr);
    Camera camera;
    RenderSnapshot first, next;
    first.FrameNumber = 1;
    renderer.ExtractSnapshot(first, camera, glm::mat4(1), false);
    const auto original = first.SpriteHandles[0];

    SECTION("Remove and re-add the component before extracting")
    {
        entity.RemoveComponent<SpriteRendererComponent>();
        entity.AddComponent<SpriteRendererComponent>();
    }
    SECTION("Destroy and reuse the entity slot before extracting")
    {
        const auto entityIndex = entt::to_entity(entity.GetHandle());
        scene->DestroyEntity(entity);
        entity = scene->CreateEntity("Replacement sprite");
        REQUIRE(entt::to_entity(entity.GetHandle()) == entityIndex);
        entity.AddComponent<SpriteRendererComponent>();
    }

    next.FrameNumber = 2;
    renderer.ExtractSnapshot(next, camera, glm::mat4(1), false);
    REQUIRE(next.SpriteHandles.Size() == 1);
    CHECK(next.SpriteHandles[0] != original);
    REQUIRE(next.RenderWorld2DChanges.Size() == 2);
    CHECK(next.RenderWorld2DChanges[0].Type == RenderChange2DType::Destroy);
    CHECK(next.RenderWorld2DChanges[0].Handle == original);
    CHECK(next.RenderWorld2DChanges[1].Type == RenderChange2DType::Create);
    CHECK(next.RenderWorld2DChanges[1].Handle == next.SpriteHandles[0]);
    CHECK(first.RenderWorld2DChanges[0].Handle == original);
    CHECK(first.RenderWorld2DChanges[0].Type == RenderChange2DType::Create);

    next.FrameNumber = 3;
    renderer.ExtractSnapshot(next, camera, glm::mat4(1), false);
    CHECK(next.RenderWorld2DChanges.Empty());
}

TEST_CASE("2D extraction reuses sparse tracking pages without warm allocations", "[Renderer][2D][SceneSync][Memory]")
{
    const auto scene = CreateRef<Scene>(false);
    Vector<Entity> sprites;
    // Leave gaps between sprites and cross several tracking-page boundaries.
    for (uint32_t index = 0; index < 4100; ++index)
    {
        Entity entity = scene->CreateEntity("Sparse sprite scene");
        if (index % 1023 == 0)
        {
            entity.AddComponent<SpriteRendererComponent>();
            sprites.push_back(entity);
        }
    }
    SceneRenderer renderer(scene, nullptr);
    Camera camera;
    RenderSnapshot snapshot;
    const auto extract = [&]() {
        ++snapshot.FrameNumber;
        for (const Entity entity : sprites)
            entity.GetComponent<SpriteRendererComponent>().Color.r = float(snapshot.FrameNumber % 2);
        renderer.ExtractSnapshot(snapshot, camera, glm::mat4(1), false);
    };
    extract();
    extract();
    const auto before = Memory::GetThreadAllocationSnapshot();
    for (uint32_t frame = 0; frame < 20; ++frame)
        extract();
    const auto delta = Memory::GetThreadAllocationDelta(before, Memory::GetThreadAllocationSnapshot());
    CHECK(delta.AllocationCount == 0);
    REQUIRE(snapshot.SpriteHandles.Size() == sprites.size());
    CHECK(snapshot.Sprites.RetainedSize() == 0);
    REQUIRE(snapshot.RenderWorld2DChanges.Size() == sprites.size());

    sprites[1].RemoveComponent<SpriteRendererComponent>();
    sprites[3].RemoveComponent<SpriteRendererComponent>();
    ++snapshot.FrameNumber;
    renderer.ExtractSnapshot(snapshot, camera, glm::mat4(1), false);
    CHECK(snapshot.SpriteHandles.Size() == sprites.size() - 2);
    REQUIRE(snapshot.RenderWorld2DChanges.Size() == 2);
    CHECK(snapshot.RenderWorld2DChanges[0].Type == RenderChange2DType::Destroy);
    CHECK(snapshot.RenderWorld2DChanges[1].Type == RenderChange2DType::Destroy);

    sprites[1].AddComponent<SpriteRendererComponent>();
    sprites[3].AddComponent<SpriteRendererComponent>();
    ++snapshot.FrameNumber;
    renderer.ExtractSnapshot(snapshot, camera, glm::mat4(1), false);
    CHECK(snapshot.SpriteHandles.Size() == sprites.size());
    REQUIRE(snapshot.RenderWorld2DChanges.Size() == 2);
    CHECK(snapshot.RenderWorld2DChanges[0].Type == RenderChange2DType::Create);
    CHECK(snapshot.RenderWorld2DChanges[1].Type == RenderChange2DType::Create);
}

TEST_CASE("Sprite copies get distinct identities while moves retain them", "[Renderer][2D][SceneSync]")
{
    SpriteRendererComponent original;
    original.Color = { .2f, .3f, .4f, .5f };
    original.SortingLayer = -2;
    original.OrderInLayer = 7;
    const auto identity = original.InstanceId;
    SpriteRendererComponent copy(original);
    CHECK(copy.InstanceId != identity);
    SpriteRendererComponent moved(std::move(original));
    CHECK(moved.InstanceId == identity);
    CHECK(moved.Color == copy.Color);
    CHECK(moved.SortingLayer == -2);
    CHECK(moved.OrderInLayer == 7);

    SpriteRendererComponent destination;
    const auto destinationIdentity = destination.InstanceId;
    destination = copy;
    CHECK(destination.InstanceId == destinationIdentity);
    destination = std::move(moved);
    CHECK(destination.InstanceId == identity);
    CHECK(destination.Color == copy.Color);
}

TEST_CASE("Compact sprite snapshots resolve legacy draw data from their render-thread mirror", "[Renderer][2D][SceneSync]")
{
    const auto scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Compact sprite");
    entity.GetTransform().SetPosition({ 3, 4, 5 });
    entity.GetTransform().SetScale({ 2, 7, 1 });
    entity.AddComponent<SpriteRendererComponent>().Color = { .2f, .3f, .4f, .5f };
    SceneRenderer renderer(scene, nullptr);
    Camera camera;
    RenderSnapshot snapshot;
    renderer.ExtractSnapshot(snapshot, camera, glm::mat4(1), false);
    REQUIRE(snapshot.SpriteHandles.Size() == 1);
    REQUIRE(snapshot.Sprites.Empty());
    const auto handle = snapshot.SpriteHandles[0];
    GpuWorld2D mirror;
    mirror.Apply({ snapshot.RenderWorld2DChanges.begin(), snapshot.RenderWorld2DChanges.Size() });
    RenderableSprite resolved;
    REQUIRE(mirror.GetSprite(handle, resolved));
    CHECK(resolved.Handle == handle);
    CHECK(resolved.WorldMatrix == entity.GetWorldMatrix());
    CHECK(resolved.Color == entity.GetComponent<SpriteRendererComponent>().Color);
    CHECK(resolved.EntityId == static_cast<int32_t>(entt::to_integral(entity.GetHandle()) + 1));

    entity.GetTransform().SetPosition({ -3, 0, 5 });
    ++snapshot.FrameNumber;
    renderer.ExtractSnapshot(snapshot, camera, glm::mat4(1), false);
    // Queued snapshots do not mutate the render-thread copy before application.
    REQUIRE(mirror.GetSprite(handle, resolved));
    CHECK(resolved.WorldMatrix[3].x == 3);
    mirror.Apply({ snapshot.RenderWorld2DChanges.begin(), snapshot.RenderWorld2DChanges.Size() });
    REQUIRE(mirror.GetSprite(handle, resolved));
    CHECK(resolved.WorldMatrix[3].x == -3);

    scene->DestroyEntity(entity);
    ++snapshot.FrameNumber;
    renderer.ExtractSnapshot(snapshot, camera, glm::mat4(1), false);
    mirror.Apply({ snapshot.RenderWorld2DChanges.begin(), snapshot.RenderWorld2DChanges.Size() });
    CHECK_FALSE(mirror.GetSprite(handle, resolved));
}

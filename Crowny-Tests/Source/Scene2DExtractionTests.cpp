#include <catch2/catch_test_macros.hpp>

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/Camera.h"
#include "Crowny/Renderer/GpuWorld2D.h"
#include "Crowny/Renderer/RenderSnapshot.h"
#include "Crowny/Scene/Scene.h"
#include "Crowny/Scene/SceneRenderer.h"

using namespace Crowny;

namespace
{
    class ExtractionTexture2D final : public Texture
    {
    public:
        ExtractionTexture2D() : Texture(TextureDesc{}, true) {}
        PixelData Lock(GpuLockOptions, uint32_t, uint32_t, uint32_t) override { return {}; }
        void Unlock() override {}
        void ReadData(PixelData&, uint32_t, uint32_t, uint32_t) override {}
        bool ReadPixel(uint32_t, uint32_t, void*, size_t, uint32_t, uint32_t, uint32_t) override { return false; }
        void WriteData(const PixelData&, uint32_t, uint32_t, uint32_t) override {}
    };
} // namespace

TEST_CASE("2D extraction preserves fragmented textures through direct edits and reimport", "[Renderer][2D][SceneSync]")
{
    AssetManager assets;
    Array<AssetHandle<Texture>, 12> textures;
    for (auto& texture : textures)
        texture = static_asset_cast<Texture>(assets.CreateAssetHandle(CreateRef<ExtractionTexture2D>()));
    const auto scene = CreateRef<Scene>(false);
    Array<Entity, 37> entities;
    for (size_t index = 0; index < entities.size(); ++index)
    {
        Entity entity = scene->CreateEntity("Fragmented sprite");
        auto& sprite = entity.AddComponent<SpriteRendererComponent>();
        sprite.Texture = textures[index % textures.size()];
        sprite.Color = { float(index) / float(entities.size()), 0.25f, 0.75f, 0.5f };
        sprite.SortingLayer = int32_t(index % 3) - 1;
        sprite.OrderInLayer = int32_t(index);
        entities[index] = entity;
    }
    SceneRenderer renderer(scene, nullptr);
    Camera camera;
    RenderSnapshot first, next;
    first.FrameNumber = 1;
    renderer.ExtractSnapshot(first, camera, glm::mat4(1), false);
    REQUIRE(first.RenderWorld2DChanges.Size() == entities.size());
    Array<RenderHandle2D, 37> handles;
    for (size_t index = 0; index < entities.size(); ++index)
    {
        const auto objectID = uint32_t(entt::to_integral(entities[index].GetHandle())) + 1u;
        const auto found = std::find_if(first.RenderWorld2DChanges.begin(), first.RenderWorld2DChanges.end(),
                                        [&](const auto& change) { return change.ObjectID.Value == objectID; });
        REQUIRE(found != first.RenderWorld2DChanges.end());
        handles[index] = found->Handle;
        CHECK(found->TextureResource.Get() == textures[index % textures.size()].Get());
        CHECK(found->Data.Color == entities[index].GetComponent<SpriteRendererComponent>().Color);
        CHECK(found->SortingLayer == int32_t(index % 3) - 1);
        CHECK(found->OrderInLayer == int32_t(index));
    }
    GpuWorld2D mirror;
    mirror.Apply({ first.RenderWorld2DChanges.begin(), first.RenderWorld2DChanges.Size() });
    const Ref<Texture> oldTexture = textures[4].GetInternalPtr();
    const auto replacement = CreateRef<ExtractionTexture2D>();
    const auto reimported = assets.CreateAssetHandle(replacement, textures[4].GetUUID());
    REQUIRE(reimported.Get() == replacement.Get());
    entities[0].GetComponent<SpriteRendererComponent>().Texture = textures[7];
    entities[1].GetComponent<SpriteRendererComponent>().Texture = nullptr;
    next.FrameNumber = 2;
    renderer.ExtractSnapshot(next, camera, glm::mat4(1), false);
    REQUIRE(next.RenderWorld2DChanges.Size() == 5);
    mirror.Apply({ next.RenderWorld2DChanges.begin(), next.RenderWorld2DChanges.Size() });
    for (size_t index = 0; index < entities.size(); ++index)
    {
        RenderableSprite sprite;
        REQUIRE(mirror.GetSprite(handles[index], sprite));
        CHECK(sprite.Texture == entities[index].GetComponent<SpriteRendererComponent>().Texture.GetInternalPtr());
    }
    for (const auto& change : first.RenderWorld2DChanges)
        if (change.Handle == handles[4])
            CHECK(change.TextureResource == oldTexture);
    next.FrameNumber = 3;
    renderer.ExtractSnapshot(next, camera, glm::mat4(1), false);
    CHECK(next.RenderWorld2DChanges.Empty());
}

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
    original.Size = { 2, 3 };
    original.Pivot = { 0, 1 };
    original.UvRect = { 0.25f, 0, 0.75f, 1 };
    original.FlipX = original.FlipY = true;
    original.Visible = false;
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
    CHECK(destination.Size == copy.Size);
    CHECK(destination.Pivot == copy.Pivot);
    CHECK(destination.UvRect == copy.UvRect);
    CHECK(destination.FlipX);
    CHECK(destination.FlipY);
    CHECK_FALSE(destination.Visible);
}

TEST_CASE("Sprite geometry edits retain identity and pivot-aware bounds", "[Renderer][2D][SceneSync]")
{
    const auto scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Region sprite");
    auto& sprite = entity.AddComponent<SpriteRendererComponent>();
    entity.GetTransform().SetPosition({ 3, 4, 0 });
    SceneRenderer renderer(scene, nullptr);
    Camera camera;
    RenderSnapshot snapshot;
    renderer.ExtractSnapshot(snapshot, camera, glm::mat4(1), false);
    const auto handle = snapshot.SpriteHandles[0];
    CHECK(snapshot.RenderWorld2DChanges[0].Data.Transform.ToMatrix() == entity.GetWorldMatrix());
    GpuWorld2D mirror;
    mirror.Apply({ snapshot.RenderWorld2DChanges.begin(), snapshot.RenderWorld2DChanges.Size() });

    sprite.Size = { 4, 2 };
    sprite.Pivot = { 0, 0.25f };
    sprite.UvRect = { 0.125f, 0.25f, 0.625f, 0.75f };
    sprite.FlipX = sprite.FlipY = true;
    ++snapshot.FrameNumber;
    renderer.ExtractSnapshot(snapshot, camera, glm::mat4(1), false);
    REQUIRE(snapshot.RenderWorld2DChanges.Size() == 1);
    const auto changed = snapshot.RenderWorld2DChanges[0];
    CHECK(changed.Handle == handle);
    CHECK(changed.Data.Transform.ToMatrix()[3] == glm::vec4(1, 3.5f, 0, 1));
    CHECK(changed.Data.Transform.ToMatrix()[0] == glm::vec4(4, 0, 0, 0));
    CHECK(changed.Data.Transform.ToMatrix()[1] == glm::vec4(0, 2, 0, 0));
    CHECK(changed.Data.UvRect == glm::vec4(0.625f, 0.75f, 0.125f, 0.25f));
    CHECK(changed.Data.PreviousTransform.ToMatrix() == entity.GetWorldMatrix());
    mirror.Apply({ &changed, 1 });
    RenderableSprite resolved;
    REQUIRE(mirror.GetSprite(handle, resolved));
    CHECK(resolved.UvRect == changed.Data.UvRect);
    CHECK(resolved.Visible);

    // Settle motion history, then unchanged geometry produces no uploads.
    ++snapshot.FrameNumber;
    renderer.ExtractSnapshot(snapshot, camera, glm::mat4(1), false);
    ++snapshot.FrameNumber;
    renderer.ExtractSnapshot(snapshot, camera, glm::mat4(1), false);
    CHECK(snapshot.RenderWorld2DChanges.Empty());
    sprite.Visible = false;
    ++snapshot.FrameNumber;
    renderer.ExtractSnapshot(snapshot, camera, glm::mat4(1), false);
    REQUIRE(snapshot.RenderWorld2DChanges.Size() == 1);
    CHECK_FALSE(snapshot.RenderWorld2DChanges[0].Visible);
    CHECK(snapshot.SpriteHandles[0] == handle);
    sprite.Visible = true;
    sprite.Size.x = std::numeric_limits<float>::quiet_NaN();
    ++snapshot.FrameNumber;
    renderer.ExtractSnapshot(snapshot, camera, glm::mat4(1), false);
    REQUIRE(snapshot.RenderWorld2DChanges.Size() == 1);
    CHECK_FALSE(snapshot.RenderWorld2DChanges[0].Visible);
    CHECK(snapshot.RenderWorld2DChanges[0].Data.Transform.ToMatrix() == glm::mat4(1));
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

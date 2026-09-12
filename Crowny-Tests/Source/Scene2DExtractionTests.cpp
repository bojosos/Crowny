#include <catch2/catch_test_macros.hpp>

#include "Crowny/Ecs/Components.h"
#include "Crowny/Renderer/Camera.h"
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
    REQUIRE(first.Sprites.Size() == 1);
    REQUIRE(first.RenderWorld2DChanges.Size() == 1);
    const auto handle = first.Sprites[0].Handle;
    CHECK(first.RenderWorld2DChanges[0].Type == RenderChange2DType::Create);

    RenderSnapshot second;
    second.FrameNumber = 2;
    entity.GetComponent<SpriteRendererComponent>().Color = { 0.2f, 0.3f, 0.4f, 0.5f };
    renderer.ExtractSnapshot(second, camera, glm::mat4(1), false);
    REQUIRE(second.RenderWorld2DChanges.Size() == 1);
    CHECK(second.Sprites[0].Handle == handle);
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
    REQUIRE(first.Sprites.Size() == 1);
    REQUIRE(second.Sprites.Size() == 1);
    CHECK(first.Sprites[0].Handle == second.Sprites[0].Handle);
    CHECK(first.World2DLifetime == second.World2DLifetime);
    CHECK(first.HistoryNamespace != second.HistoryNamespace);
    CHECK(second.RenderWorld2DChanges.Empty());

    renderer.SetScene(CreateRef<Scene>(false));
    second.FrameNumber = 2;
    renderer.ExtractSnapshot(second, secondCamera, glm::mat4(1), false);
    REQUIRE(second.RenderWorld2DChanges.Size() == 1);
    CHECK(second.RenderWorld2DChanges[0].Type == RenderChange2DType::Destroy);
    CHECK(second.Sprites.Empty());
    // The old queued snapshot still owns its data and owner lifetime token.
    CHECK(first.World2DLifetime != nullptr);
    CHECK(first.RenderWorld2DChanges[0].Type == RenderChange2DType::Create);
}

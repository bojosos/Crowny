#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Crowny/Assets/AssetCodecs.h"
#include "Crowny/Assets/AssetListener.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Renderer/Camera.h"
#include "Crowny/Renderer/RenderSnapshot.h"
#include "Crowny/Scene/SceneRenderer.h"
#include "Crowny/Scripting/Managed/Interop/ManagedHostBindings.h"
#include "Crowny/Serialization/SpriteAnimationSerializer.h"

using namespace Crowny;

namespace
{
    SpriteAnimationClipData TestClipData(SpriteAnimationMode mode = SpriteAnimationMode::Loop)
    {
        return { { { UUID(1, 0, 0, 0), 0.25f }, { UUID(2, 0, 0, 0), 0.5f }, { UUID(3, 0, 0, 0), 0.25f } }, mode };
    }

    struct SpriteAnimationAssets
    {
        bool OwnsListeners = !AssetListenerManager::IsStartedUp();
        bool OwnsAssets = !AssetManager::IsStartedUp();
        SpriteAnimationAssets()
        {
            if (OwnsListeners)
                AssetListenerManager::StartUp();
            if (OwnsAssets)
                AssetManager::StartUp();
        }
        ~SpriteAnimationAssets()
        {
            if (OwnsAssets)
                AssetManager::Shutdown();
            if (OwnsListeners)
                AssetListenerManager::Shutdown();
        }
    };
} // namespace

TEST_CASE("Sprite clips sample durations and ping pong endpoints", "[2D][SpriteAnimation]")
{
    SpriteAnimationClip clip;
    CHECK(clip.Sample(0) == UINT32_MAX);
    REQUIRE(clip.SetData(TestClipData()));
    CHECK(clip.GetDuration() == 1.0);
    CHECK(clip.Sample(0) == 0);
    CHECK(clip.Sample(0.25) == 1);
    CHECK(clip.Sample(0.75) == 2);
    CHECK(clip.Sample(1) == 0);
    CHECK(clip.Sample(-0.25) == 2);
    CHECK(clip.Sample(1000000000.5) == 1);
    REQUIRE(clip.SetData(TestClipData(SpriteAnimationMode::Once)));
    CHECK(clip.Sample(-1) == 0);
    CHECK(clip.Sample(1) == 2);
    REQUIRE(clip.SetData(TestClipData(SpriteAnimationMode::PingPong)));
    CHECK(clip.GetDuration() == 1.5);
    CHECK(clip.Sample(0.75) == 2);
    CHECK(clip.Sample(1) == 1);
    CHECK(clip.Sample(1.5) == 0);
    auto data = TestClipData(SpriteAnimationMode::PingPong);
    data.Frames.resize(1);
    REQUIRE(clip.SetData(data));
    CHECK(clip.GetDuration() == 0.25);
    CHECK(clip.Sample(-100) == 0);
}

TEST_CASE("Sprite playback controls and completion counts are frame rate independent", "[2D][SpriteAnimation]")
{
    SpriteAnimationClip clip;
    REQUIRE(clip.SetData(TestClipData()));
    SpriteAnimationPlayback playback;
    playback.Advance(0.5, 1, clip);
    CHECK(playback.GetTime() == 0);
    playback.Play();
    playback.Advance(1000000.25, 1, clip);
    CHECK(playback.GetTime() == 0.25);
    CHECK(playback.ConsumeCompletions() == 1000000);
    CHECK(playback.ConsumeCompletions() == 0);
    playback.Pause();
    playback.Advance(1, 1, clip);
    CHECK(playback.GetTime() == 0.25);
    playback.Seek(0.75, clip);
    playback.Play();
    playback.Advance(1, -1, clip);
    CHECK(playback.GetTime() == 0.75);
    CHECK(playback.ConsumeCompletions() == 1);
    playback.Advance(-1, 1, clip);
    playback.Advance(1, std::numeric_limits<double>::infinity(), clip);
    CHECK(playback.GetTime() == 0.75);
    playback.Seek(0.25, clip);
    playback.Advance(0.25, -1, clip);
    CHECK(playback.GetTime() == 0);
    CHECK(playback.ConsumeCompletions() == 1);
    playback.Advance(0.25, -1, clip);
    CHECK(playback.GetTime() == 0.75);
    CHECK(playback.ConsumeCompletions() == 0);
    REQUIRE(clip.SetData(TestClipData(SpriteAnimationMode::Once)));
    playback.Advance(10, 1, clip);
    CHECK(playback.GetTime() == 1);
    CHECK_FALSE(playback.IsPlaying());
    CHECK(playback.ConsumeCompletions() == 1);
    playback.Play();
    playback.Advance(3, -1, clip);
    CHECK(playback.GetTime() == 0);
    CHECK(playback.ConsumeCompletions() == 1);
    playback.Stop();
    CHECK_FALSE(playback.IsPlaying());
}

TEST_CASE("Sprite animation source and cooked assets retain frame identities", "[2D][SpriteAnimation][Serialization]")
{
    SpriteAnimationAssets runtime;
    auto clip = CreateRef<SpriteAnimationClip>();
    clip->SetName("Walk");
    REQUIRE(clip->SetData(TestClipData(SpriteAnimationMode::PingPong)));
    const auto source = SpriteAnimationSerializer(clip).SerializeToString();
    auto loaded = CreateRef<SpriteAnimationClip>();
    REQUIRE(SpriteAnimationSerializer(loaded).DeserializeFromString(source));
    CHECK(loaded->GetData() == clip->GetData());
    CHECK(loaded->GetName() == "Walk");
    const auto revision = clip->GetRevision();
    auto invalid = clip->GetData();
    invalid.Frames[1].Duration = 0;
    CHECK_FALSE(clip->SetData(invalid));
    invalid.Frames[0].Duration = std::numeric_limits<float>::max();
    invalid.Frames[1].Duration = std::numeric_limits<float>::max();
    CHECK_FALSE(clip->SetData(invalid));
    CHECK(clip->GetRevision() == revision);
    CHECK_FALSE(SpriteAnimationSerializer(clip).DeserializeFromString("Version: 1\nMode: invalid\n"));
    CHECK_FALSE(SpriteAnimationSerializer(clip).DeserializeFromString("Version: 1\nFrames: [{Sprite: invalid, Duration: wrong}]\n"));
    CHECK(clip->GetRevision() == revision);
    const Path path = fs::temp_directory_path() / ("crowny-sprite-animation-" + UuidGenerator::Generate().ToString() + ".asset");
    REQUIRE(AssetManager::Get().Save(clip, path));
    const auto cooked = AssetManager::Get().Load<SpriteAnimationClip>(path, false);
    fs::remove(path);
    REQUIRE(cooked);
    CHECK(cooked->GetData() == clip->GetData());
    CHECK(cooked->Sample(1.25) == 1);
}

TEST_CASE("Sprite animation advances through simulation and survives ECS relocation", "[2D][SpriteAnimation][SceneSync]")
{
    SpriteAnimationAssets runtime;
    const auto clip = CreateRef<SpriteAnimationClip>();
    REQUIRE(clip->SetData(TestClipData()));
    const auto scene = CreateRef<Scene>(false);
    const auto entity = scene->CreateEntity("Animated sprite");
    auto& sprite = entity.AddComponent<SpriteRendererComponent>();
    sprite.Visible = false;
    auto& animation = entity.AddComponent<SpriteAnimatorComponent>();
    animation.Clip = static_asset_cast<SpriteAnimationClip>(AssetManager::Get().CreateAssetHandle(clip));
    scene->OnUpdateRuntime(Timestep(0.5f));
    CHECK(animation.GetTime() == 0.5f);
    CHECK(sprite.Sprite.GetUUID() == UUID(2, 0, 0, 0));
    SceneRenderer renderer(scene, nullptr);
    RenderSnapshot snapshot;
    Camera camera;
    renderer.ExtractSnapshot(snapshot, camera, glm::mat4(1), false);
    renderer.ExtractSnapshot(snapshot, camera, glm::mat4(1), false);
    CHECK(animation.GetTime() == 0.5f);
    SpriteAnimatorComponent copied(animation);
    CHECK(copied.GetTime() == 0);
    const auto identity = animation.InstanceId;
    SpriteAnimatorComponent moved(std::move(animation));
    CHECK(moved.GetTime() == 0.5f);
    CHECK(moved.InstanceId == identity);
    CHECK(moved.IsPlaying());
    moved.Pause();
    moved.Advance(10);
    CHECK(moved.GetTime() == 0.5f);
    auto changed = TestClipData();
    changed.Frames[1].SpriteId = UUID(4, 0, 0, 0);
    REQUIRE(clip->SetData(changed));
    CHECK(moved.GetFrameSpriteId() == UUID(4, 0, 0, 0));
}

TEST_CASE("Managed sprite animation assets use the shared typed contract", "[2D][SpriteAnimation][Managed][Contract]")
{
    SpriteAnimationAssets runtime;
    const UUID id("11111111-1111-1111-1111-111111111111");
    cw_managed_uuid managedId{};
    std::fill(std::begin(managedId.bytes), std::end(managedId.bytes), uint8_t(0x11));
    const auto clip = CreateRef<SpriteAnimationClip>();
    REQUIRE(clip->SetData(TestClipData(SpriteAnimationMode::PingPong)));
    const auto handle = AssetManager::Get().CreateAssetHandle(clip, id);
    cw_managed_host_api api{};
    PopulateManagedHostBindings(api);
    int context = 0;
    float duration = 0;
    uint32_t count = 0;
    int32_t mode = -1;
    CHECK(api.sprite_animation_clip_get_duration(&context, managedId, &duration) == CW_MANAGED_STATUS_OK);
    CHECK(duration == 1.5f);
    CHECK(api.sprite_animation_clip_get_frame_count(&context, managedId, &count) == CW_MANAGED_STATUS_OK);
    CHECK(count == 3);
    CHECK(api.sprite_animation_clip_get_mode(&context, managedId, &mode) == CW_MANAGED_STATUS_OK);
    CHECK(mode == static_cast<int32_t>(SpriteAnimationMode::PingPong));
    CHECK(api.sprite_animation_clip_get_mode(&context, managedId, nullptr) == CW_MANAGED_STATUS_INVALID_ARGUMENT);
    CHECK(api.sprite_atlas_get_page_count(&context, managedId, &count) == CW_MANAGED_STATUS_STALE_HANDLE);
}

#include <catch2/catch_test_macros.hpp>

#include "Crowny/Assets/AssetCodecs.h"
#include "Crowny/Assets/AssetListener.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/Yaml.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Import/SpriteImporter.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/Camera.h"
#include "Crowny/Renderer/RenderSnapshot.h"
#include "Crowny/Scene/SceneRenderer.h"
#include "Crowny/Scripting/Managed/Interop/ManagedHostBindings.h"
#include "Crowny/Serialization/SceneSerializer.h"
#include "Crowny/Serialization/SpriteSerializer.h"
#include "Editor/AssetDataInspectorTransaction.h"

using namespace Crowny;

namespace
{
    struct SpriteAssetRuntime
    {
        bool OwnsListeners = !AssetListenerManager::IsStartedUp();
        bool OwnsAssets = !AssetManager::IsStartedUp();
        SpriteAssetRuntime()
        {
            if (OwnsListeners)
                AssetListenerManager::StartUp();
            if (OwnsAssets)
                AssetManager::StartUp();
        }
        ~SpriteAssetRuntime()
        {
            if (OwnsAssets)
                AssetManager::Shutdown();
            if (OwnsListeners)
                AssetListenerManager::Shutdown();
        }
    };

    class SpriteTestTexture final : public Texture
    {
    public:
        SpriteTestTexture() : Texture(Description(), true) {}
        PixelData Lock(GpuLockOptions, uint32_t, uint32_t, uint32_t) override { return {}; }
        void Unlock() override {}
        void ReadData(PixelData&, uint32_t, uint32_t, uint32_t) override {}
        bool ReadPixel(uint32_t, uint32_t, void*, size_t, uint32_t, uint32_t, uint32_t) override { return false; }
        void WriteData(const PixelData&, uint32_t, uint32_t, uint32_t) override {}

    private:
        static TextureDesc Description()
        {
            TextureDesc desc;
            desc.Width = 400;
            desc.Height = 200;
            return desc;
        }
    };
} // namespace

TEST_CASE("Sprite assets resolve pixel scale and retain missing texture identities", "[2D][Sprite][Assets]")
{
    SpriteAssetRuntime runtime;
    auto& manager = AssetManager::Get();
    auto sprite = CreateRef<Sprite>();
    SpriteData data;
    data.TextureId = UuidGenerator::Generate();
    data.UvRect = { 0.25f, 0, 0.75f, 0.5f };
    data.Pivot = { 0, 1 };
    REQUIRE(sprite->SetData(data));
    CHECK(sprite->GetTexture().GetUUID() == data.TextureId);
    CHECK_FALSE(sprite->GetGeometry().IsValid());

    const auto texture = manager.CreateAssetHandle(CreateRef<SpriteTestTexture>(), data.TextureId);
    REQUIRE(sprite->GetTexture());
    CHECK(sprite->GetGeometry().Size == glm::vec2(2, 1));
    SpriteRendererComponent component;
    component.Sprite = static_asset_cast<Sprite>(manager.CreateAssetHandle(sprite));
    CHECK(component.ResolveGeometry().Size == glm::vec2(2, 1));
    CHECK(component.ResolveGeometry().Pivot == data.Pivot);
    CHECK(component.GetTexture().GetUUID() == data.TextureId);
    component.Size = { 3, 4 };
    component.Pivot = { 0.75f, 0.25f };
    CHECK(component.ResolveGeometry().Size == glm::vec2(2, 1));
    component.UseSpriteSize = component.UseSpritePivot = false;
    CHECK(component.ResolveGeometry().Size == component.Size);
    CHECK(component.ResolveGeometry().Pivot == component.Pivot);
    CHECK(component.ResolveGeometry().UvRect == data.UvRect);

    data.OriginalSize = { 300, 200 };
    REQUIRE(sprite->SetData(data));
    CHECK(sprite->GetGeometry().Size == glm::vec2(3, 2));
    component.Sprite = static_asset_cast<Sprite>(manager.GetAssetHandle(UuidGenerator::Generate()));
    component.Texture = static_asset_cast<Texture>(texture);
    CHECK_FALSE(component.HasValidGeometry());
    CHECK_FALSE(component.GetTexture());
    component.Sprite = nullptr;
    CHECK(component.HasValidGeometry());
    CHECK(component.GetTexture().GetUUID() == texture.GetUUID());
    component.Sprite = static_asset_cast<Sprite>(texture);
    CHECK_FALSE(component.HasValidGeometry());
    CHECK_FALSE(component.GetTexture());
    manager.CreateAssetHandle(CreateRef<Sprite>(), data.TextureId);
    CHECK_FALSE(sprite->GetTexture());
    CHECK_FALSE(sprite->GetGeometry().IsValid());
}

TEST_CASE("Sprite edits and source imports reject invalid data transactionally", "[2D][Sprite][Assets]")
{
    SpriteAssetRuntime runtime;
    auto sprite = CreateRef<Sprite>();
    sprite->SetName("Original");
    const SpriteData before = sprite->GetData();
    SpriteData invalid = before;
    AssetHandle<Asset> wrongKind;
    SECTION("Zero pixel scale") { invalid.PixelsPerUnit = 0; }
    SECTION("Nonfinite pivot") { invalid.Pivot.x = std::numeric_limits<float>::infinity(); }
    SECTION("Empty region") { invalid.UvRect.z = 0; }
    SECTION("Negative original size") { invalid.OriginalSize.x = -1; }
    SECTION("Oversized borders")
    {
        invalid.OriginalSize = { 8, 8 };
        invalid.Borders = { 5, 0, 5, 0 };
    }
    SECTION("Wrong asset kind")
    {
        wrongKind = AssetManager::Get().CreateAssetHandle(sprite);
        invalid.TextureId = wrongKind.GetUUID();
    }
    CHECK_FALSE(sprite->SetData(invalid));
    CHECK(sprite->GetData() == before);
    SpriteSerializer serializer(sprite);
    CHECK_FALSE(serializer.DeserializeFromString("Version: 1\nName: Changed\nPixelsPerUnit: 0\n"));
    CHECK_FALSE(serializer.DeserializeFromString("Version: 2\n"));
    CHECK_FALSE(serializer.DeserializeFromString("Version: 1\nUvRect: [1, 2]\n"));
    CHECK(sprite->GetData() == before);
    CHECK(sprite->GetName() == "Original");
}

TEST_CASE("Sprite source and cooked assets preserve metadata and unresolved references", "[2D][Sprite][Serialization]")
{
    SpriteAssetRuntime runtime;
    auto sprite = CreateRef<Sprite>();
    SpriteData data;
    data.TextureId = UuidGenerator::Generate();
    data.UvRect = { 0.25f, 0.125f, 0.75f, 0.875f };
    data.Pivot = { -0.25f, 1.25f };
    data.OriginalSize = { 64, 96 };
    data.Borders = { 1, 2, 3, 4 };
    data.PixelsPerUnit = 32;
    REQUIRE(sprite->SetData(data));
    sprite->SetName("Region");
    const Path path = fs::temp_directory_path() / ("crowny-sprite-" + UuidGenerator::Generate().ToString() + ".cwsprite");
    Ref<Sprite> loaded;
    SECTION("Source importer")
    {
        REQUIRE(SpriteSerializer(sprite).Serialize(path));
        SpriteImporter importer;
        CHECK(importer.IsExtensionSupported("cwsprite"));
        loaded = StaticRefCast<Sprite>(importer.Import(path, nullptr));
    }
    SECTION("Compiled asset")
    {
        REQUIRE(AssetManager::Get().Save(sprite, path));
        AssetFileHeader header;
        REQUIRE(PeekAssetHeader(path, header));
        CHECK(header.Type == AssetType::Sprite);
        CHECK(header.Version == SPRITE_FORMAT_VERSION);
        loaded = AssetManager::Get().Load<Sprite>(path, false).GetInternalPtr();
    }
    fs::remove(path);
    REQUIRE(loaded);
    CHECK(loaded->GetName() == sprite->GetName());
    CHECK(loaded->GetData() == data);
    CHECK(loaded->GetTexture().GetUUID() == data.TextureId);
    CHECK_FALSE(loaded->GetTexture());
}

TEST_CASE("Cold sprite texture references cannot recursively load sprite assets", "[2D][Sprite][Serialization]")
{
    SpriteAssetRuntime runtime;
    auto& manager = AssetManager::Get();
    const auto sprite = CreateRef<Sprite>();
    SpriteData data;
    data.TextureId = UuidGenerator::Generate();
    REQUIRE(sprite->SetData(data));
    const Path path = fs::temp_directory_path() / ("crowny-sprite-cycle-" + data.TextureId.ToString() + ".asset");
    REQUIRE(manager.Save(sprite, path));
    const auto manifest = CreateRef<AssetManifest>("sprite-cycle-test");
    manifest->RegisterAsset(data.TextureId, path);
    manager.RegisterAssetManifest(manifest);
    CHECK_FALSE(manager.LoadFromUUID(data.TextureId, false));
    manager.UnregisterAssetManifest(manifest);
    fs::remove(path);
}

TEST_CASE("Sprite asset edits update retained instances without touching entities", "[2D][Sprite][SceneSync]")
{
    SpriteAssetRuntime runtime;
    auto& assets = AssetManager::Get();
    const auto texture = assets.CreateAssetHandle(CreateRef<SpriteTestTexture>());
    const auto asset = CreateRef<Sprite>();
    SpriteData data;
    data.TextureId = texture.GetUUID();
    REQUIRE(asset->SetData(data));
    const auto scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Authored sprite");
    auto& component = entity.AddComponent<SpriteRendererComponent>();
    component.Sprite = static_asset_cast<Sprite>(assets.CreateAssetHandle(asset));
    SceneRenderer renderer(scene, nullptr);
    Camera camera;
    RenderSnapshot first, next;
    first.FrameNumber = 1;
    renderer.ExtractSnapshot(first, camera, glm::mat4(1), false);
    REQUIRE(first.RenderWorld2DChanges.Size() == 1);
    const auto handle = first.RenderWorld2DChanges[0].Handle;
    data.UvRect = { 0, 0, 0.5f, 0.5f };
    data.Pivot = { 0, 0 };
    REQUIRE(asset->SetData(data));
    next.FrameNumber = 2;
    renderer.ExtractSnapshot(next, camera, glm::mat4(1), false);
    REQUIRE(next.RenderWorld2DChanges.Size() == 1);
    CHECK(next.RenderWorld2DChanges[0].Handle == handle);
    CHECK(first.RenderWorld2DChanges[0].Data.UvRect == glm::vec4(0, 0, 1, 1));
    CHECK(next.RenderWorld2DChanges[0].Data.UvRect == data.UvRect);
    next.FrameNumber = 3;
    renderer.ExtractSnapshot(next, camera, glm::mat4(1), false);
    REQUIRE(next.RenderWorld2DChanges.Size() == 1); // Previous transform settles once.
    next.FrameNumber = 4;
    renderer.ExtractSnapshot(next, camera, glm::mat4(1), false);
    CHECK(next.RenderWorld2DChanges.Empty());

    const auto replacement = CreateRef<Sprite>();
    data.PixelsPerUnit = 50;
    REQUIRE(replacement->SetData(data));
    assets.CreateAssetHandle(replacement, component.Sprite.GetUUID());
    next.FrameNumber = 5;
    renderer.ExtractSnapshot(next, camera, glm::mat4(1), false);
    REQUIRE(next.RenderWorld2DChanges.Size() == 1);
    CHECK(next.RenderWorld2DChanges[0].Handle == handle);
    CHECK(component.ResolveGeometry().Size == glm::vec2(4, 2));
}

TEST_CASE("Sprite dependencies include authored assets and legacy textures once", "[2D][Sprite][Build]")
{
    const UUID sprite("11111111-1111-1111-1111-111111111111");
    const UUID texture("22222222-2222-2222-2222-222222222222");
    const auto source =
      YAML::Load("Entities:\n"
                 "  - SpriteRendererComponent: {Sprite: 11111111-1111-1111-1111-111111111111, Texture: 22222222-2222-2222-2222-222222222222}\n"
                 "  - SpriteRendererComponent: {Texture: 22222222-2222-2222-2222-222222222222}\n"
                 "  - SpriteRendererComponent: {}\n");
    const auto dependencies = SceneSerializer::GatherSpriteDependencies(source);
    REQUIRE(dependencies.size() == 2);
    CHECK(std::find(dependencies.begin(), dependencies.end(), sprite) != dependencies.end());
    CHECK(std::find(dependencies.begin(), dependencies.end(), texture) != dependencies.end());
}

TEST_CASE("Sprite asset undo retains both metadata and pending saves", "[2D][Sprite][Editor][Undo]")
{
    const auto sprite = CreateRef<Sprite>();
    const auto saves = CreateRef<AssetSaveTracker>();
    AssetDataInspectorTransaction<Sprite, SpriteData> transaction;
    transaction.Capture("Assets/Region.cwsprite", sprite, saves);
    CHECK_FALSE(transaction.Build());
    auto data = sprite->GetData();
    data.PixelsPerUnit = 32;
    data.Pivot = { 0, 0 };
    REQUIRE(sprite->SetData(data));
    const auto action = transaction.Build();
    REQUIRE(action);
    transaction.Reset();
    action->Revert();
    CHECK(sprite->GetData().PixelsPerUnit == 100);
    CHECK(sprite->GetData().Pivot == glm::vec2(0.5f));
    auto save = saves->TakeReady();
    REQUIRE(save.has_value());
    CHECK(save->Filepath == Path("Assets/Region.cwsprite"));
    saves->Resolve(save->Filepath, true);
    action->Commit();
    CHECK(sprite->GetData() == data);
    CHECK(saves->TakeReady().has_value());
}

TEST_CASE("Managed sprite assets expose metadata and reject the wrong asset kind", "[2D][Sprite][Managed][Contract]")
{
    SpriteAssetRuntime runtime;
    const UUID spriteId("11111111-1111-1111-1111-111111111111");
    const UUID textureId("22222222-2222-2222-2222-222222222222");
    cw_managed_uuid managedSprite{}, managedTexture{};
    std::fill(std::begin(managedSprite.bytes), std::end(managedSprite.bytes), uint8_t(0x11));
    std::fill(std::begin(managedTexture.bytes), std::end(managedTexture.bytes), uint8_t(0x22));
    auto& manager = AssetManager::Get();
    const auto texture = manager.CreateAssetHandle(CreateRef<SpriteTestTexture>(), textureId);
    auto sprite = CreateRef<Sprite>();
    auto data = sprite->GetData();
    data.TextureId = textureId;
    data.PixelsPerUnit = 50;
    data.Pivot = { 0.25f, 0.75f };
    REQUIRE(sprite->SetData(data));
    const auto handle = manager.CreateAssetHandle(sprite, spriteId);
    cw_managed_host_api api{};
    PopulateManagedHostBindings(api);
    int context = 0;
    float scale = 0;
    CHECK(api.sprite_get_pixels_per_unit(&context, managedSprite, &scale) == CW_MANAGED_STATUS_OK);
    CHECK(scale == 50);
    cw_managed_vec2 size{};
    CHECK(api.sprite_get_size(&context, managedSprite, &size) == CW_MANAGED_STATUS_OK);
    CHECK(size.x == 8);
    CHECK(size.y == 4);
    CHECK(api.sprite_get_pivot(&context, managedSprite, &size) == CW_MANAGED_STATUS_OK);
    CHECK(size.x == 0.25f);
    CHECK(size.y == 0.75f);
    cw_managed_uuid resolved{};
    CHECK(api.sprite_get_texture(&context, managedSprite, &resolved) == CW_MANAGED_STATUS_OK);
    CHECK(std::memcmp(resolved.bytes, managedTexture.bytes, sizeof(resolved.bytes)) == 0);
    CHECK(api.sprite_get_size(&context, managedTexture, &size) == CW_MANAGED_STATUS_STALE_HANDLE);
    CHECK(api.sprite_get_size(&context, managedSprite, nullptr) == CW_MANAGED_STATUS_INVALID_ARGUMENT);
}

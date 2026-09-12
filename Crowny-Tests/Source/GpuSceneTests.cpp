#include <catch2/catch_test_macros.hpp>

#include "Crowny/Assets/AssetListener.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Renderer/GpuScene.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/ShaderVariation.h"

using namespace Crowny;

namespace
{
    class ScopedGpuSceneAssetModules
    {
    public:
        ScopedGpuSceneAssetModules()
        {
            if (AssetListenerManager::TryGet() == nullptr)
            {
                AssetListenerManager::StartUp();
                m_OwnsListenerManager = true;
            }
            if (AssetManager::TryGet() == nullptr)
            {
                AssetManager::StartUp();
                m_OwnsAssetManager = true;
            }
        }

        ~ScopedGpuSceneAssetModules()
        {
            if (m_OwnsAssetManager)
                AssetManager::Shutdown();
            if (m_OwnsListenerManager)
                AssetListenerManager::Shutdown();
        }

    private:
        bool m_OwnsListenerManager = false;
        bool m_OwnsAssetManager = false;
    };
    class SceneTestTexture final : public Texture
    {
    public:
        SceneTestTexture() : Texture(TextureDesc{}, true) {}
        PixelData Lock(GpuLockOptions, uint32_t, uint32_t, uint32_t) override { return {}; }
        void Unlock() override {}
        void ReadData(PixelData&, uint32_t, uint32_t, uint32_t) override {}
        bool ReadPixel(uint32_t, uint32_t, void*, size_t, uint32_t, uint32_t, uint32_t) override { return false; }
        void WriteData(const PixelData&, uint32_t, uint32_t, uint32_t) override {}
    };

    struct ScopedSceneTextures
    {
        Array<Ref<Texture>, 4> Previous{ Texture::WHITE, Texture::BLACK, Texture::NORMAL, Texture::MISSING };
        ScopedSceneTextures()
        {
            Texture::WHITE = CreateRef<SceneTestTexture>();
            Texture::BLACK = CreateRef<SceneTestTexture>();
            Texture::NORMAL = CreateRef<SceneTestTexture>();
            Texture::MISSING = CreateRef<SceneTestTexture>();
        }
        ~ScopedSceneTextures()
        {
            Texture::WHITE = Previous[0];
            Texture::BLACK = Previous[1];
            Texture::NORMAL = Previous[2];
            Texture::MISSING = Previous[3];
        }
    };

    AssetHandle<Material> SceneMaterial(const AssetHandle<Texture>& texture)
    {
        AssetManager& manager = AssetManager::Get();
        ShaderDesc desc;
        desc.Techniques = { ShaderTechnique::Create({ "material_model=standard" }, {}, {}) };
        const auto shader = static_asset_cast<Shader>(manager.CreateAssetHandle(Shader::Create(desc)));
        const Ref<Material> material = Material::Create(shader);
        material->SetTexture("albedoMap", texture);
        return static_asset_cast<Material>(manager.CreateAssetHandle(material));
    }

    AssetHandle<Texture> SceneTexture() { return static_asset_cast<Texture>(AssetManager::Get().CreateAssetHandle(CreateRef<SceneTestTexture>())); }
} // namespace

TEST_CASE("Material parameter changes preserve texture slots and untouched records", "[Renderer][GpuScene][Bindless]")
{
    ScopedGpuSceneAssetModules modules;
    ScopedSceneTextures defaults;
    const auto first = SceneMaterial(SceneTexture());
    const auto second = SceneMaterial(SceneTexture());
    GpuScene scene(false, 8);
    const RenderMaterialResourceChange create[] = { { 1, 1, RenderResourceChangeType::CreateOrUpdate, first },
                                                    { 7, 1, RenderResourceChangeType::CreateOrUpdate, second } };
    scene.ApplyResources(nullptr, 0, create, 2);
    REQUIRE(scene.GetMaterialData(7) != nullptr);
    const GpuMaterialData untouched = *scene.GetMaterialData(7);
    const uint32_t firstTexture = scene.GetMaterialData(1)->TextureIndices0.x;
    const uint64_t version = scene.GetBindlessTextureVersion();
    Vector<BindlessResourceUpdate> updates;
    scene.DrainBindlessTextureUpdates(updates);

    scene.Apply(nullptr, 0, nullptr, 0);
    first->SetAlphaMode(AlphaMode::Mask);
    const RenderMaterialResourceChange change{ 1, 2, RenderResourceChangeType::CreateOrUpdate, first };
    scene.ApplyResources(nullptr, 0, &change, 1);
    CHECK(scene.GetStats().MaterialRecordsUpdated == 1);
    CHECK(scene.GetBindlessTextureVersion() == version);
    CHECK(scene.GetMaterialData(1)->TextureIndices0.x == firstTexture);
    CHECK(std::memcmp(scene.GetMaterialData(7), &untouched, sizeof(untouched)) == 0);
    scene.DrainBindlessTextureUpdates(updates);
    CHECK(updates.empty());

    scene.Apply(nullptr, 0, nullptr, 0);
    scene.ApplyResources(nullptr, 0, &change, 1);
    CHECK(scene.GetStats().MaterialRecordsUpdated == 0);
    CHECK(scene.GetStats().MaterialRanges == 0);

    const RenderMaterialResourceChange sameData{ 1, 3, RenderResourceChangeType::CreateOrUpdate, first };
    scene.ApplyResources(nullptr, 0, &sameData, 1);
    CHECK(scene.GetStats().MaterialRecordsUpdated == 0);
    CHECK(scene.GetBindlessTextureVersion() == version);
}

TEST_CASE("Shared textures stay resident until the last material releases them", "[Renderer][GpuScene][Bindless]")
{
    ScopedGpuSceneAssetModules modules;
    ScopedSceneTextures defaults;
    const auto texture = SceneTexture();
    const auto first = SceneMaterial(texture);
    const auto second = SceneMaterial(texture);
    GpuScene scene(false, 6);
    const RenderMaterialResourceChange create[] = { { 1, 1, RenderResourceChangeType::CreateOrUpdate, first },
                                                    { 2, 1, RenderResourceChangeType::CreateOrUpdate, second } };
    scene.ApplyResources(nullptr, 0, create, 2);
    const uint32_t shared = scene.GetMaterialData(2)->TextureIndices0.x;
    REQUIRE(shared != 0);
    CHECK(scene.GetMaterialData(1)->TextureIndices0.x == shared);
    const uint64_t version = scene.GetBindlessTextureVersion();

    const RenderMaterialResourceChange destroyFirst{ 1, 2, RenderResourceChangeType::Destroy, {} };
    scene.ApplyResources(nullptr, 0, &destroyFirst, 1);
    CHECK(scene.GetBindlessTextures()[shared] == texture.GetInternalPtr());
    CHECK(scene.GetBindlessTextureVersion() == version);

    const RenderMaterialResourceChange destroySecond{ 2, 2, RenderResourceChangeType::Destroy, {} };
    scene.ApplyResources(nullptr, 0, &destroySecond, 1);
    CHECK(scene.GetBindlessTextures()[shared] == Texture::MISSING);
    CHECK(scene.GetStats().BindlessTextureCount == 4);
}

TEST_CASE("Texture overflow uses slot zero and recovers when capacity is released", "[Renderer][GpuScene][Bindless]")
{
    ScopedGpuSceneAssetModules modules;
    ScopedSceneTextures defaults;
    const auto first = SceneMaterial(SceneTexture());
    const auto secondTexture = SceneTexture();
    const auto second = SceneMaterial(secondTexture);
    GpuScene scene(false, 5);
    const RenderMaterialResourceChange create[] = { { 1, 1, RenderResourceChangeType::CreateOrUpdate, first },
                                                    { 2, 1, RenderResourceChangeType::CreateOrUpdate, second } };
    scene.ApplyResources(nullptr, 0, create, 2);
    CHECK(scene.GetStats().BindlessTextureCapacity == 5);
    CHECK(scene.GetBindlessTextures().size() == 5);
    CHECK(scene.GetStats().BindlessTextureOverflowMaterials == 1);
    CHECK(scene.GetMaterialData(2)->TextureIndices0.x == 0);

    const RenderMaterialResourceChange destroy{ 1, 2, RenderResourceChangeType::Destroy, {} };
    scene.ApplyResources(nullptr, 0, &destroy, 1);
    const uint32_t recovered = scene.GetMaterialData(2)->TextureIndices0.x;
    REQUIRE(recovered != 0);
    REQUIRE(recovered < scene.GetBindlessTextures().size());
    CHECK(scene.GetBindlessTextures()[recovered] == secondTexture.GetInternalPtr());
    CHECK(scene.GetStats().BindlessTextureOverflowMaterials == 0);

    const auto replacement = SceneTexture();
    second->SetTexture("albedoMap", replacement);
    const RenderMaterialResourceChange replace{ 2, 2, RenderResourceChangeType::CreateOrUpdate, second };
    scene.ApplyResources(nullptr, 0, &replace, 1);
    CHECK(scene.GetStats().BindlessTextureOverflowMaterials == 0);
    CHECK(scene.GetBindlessTextures()[scene.GetMaterialData(2)->TextureIndices0.x] == replacement.GetInternalPtr());
    CHECK(scene.GetBindlessTextures().size() == 5);
}

TEST_CASE("A one-slot texture table and scene reset retain valid fallback indices", "[Renderer][GpuScene][Bindless]")
{
    ScopedGpuSceneAssetModules modules;
    ScopedSceneTextures defaults;
    const auto material = SceneMaterial(SceneTexture());
    GpuScene scene(false, 1);
    const RenderMaterialResourceChange create{ 1, 1, RenderResourceChangeType::CreateOrUpdate, material };
    scene.ApplyResources(nullptr, 0, &create, 1);
    CHECK(scene.GetBindlessTextures().size() == 1);
    CHECK(scene.GetMaterialData(0)->TextureIndices0 == glm::uvec4(0));
    CHECK(scene.GetMaterialData(1)->TextureIndices0 == glm::uvec4(0));
    const uint64_t version = scene.GetBindlessTextureVersion();
    scene.Reset();
    scene.ApplyResources(nullptr, 0, &create, 1);
    CHECK(scene.GetBindlessTextureVersion() > version);
    CHECK(scene.GetBindlessTextures().size() == 1);
}

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
    CHECK_FALSE(scene.HasToonOutlineMaterials());
    CHECK_FALSE(scene.HasToonSilhouetteMaterials());
}

TEST_CASE("GPU scene applies legacy toon variations before classifying draw bins", "[Renderer][GpuScene][Materials][Toon]")
{
    ScopedGpuSceneAssetModules modules;
    AssetManager& manager = AssetManager::Get();
    ShaderVariation toonVariation;
    toonVariation.Set("TOON", true);
    const Ref<ShaderTechnique> technique = ShaderTechnique::Create({ "material_model=standard" }, toonVariation, {});
    ShaderDesc shaderDesc;
    shaderDesc.Techniques = { technique };
    const AssetHandle<Shader> shader = static_asset_cast<Shader>(manager.CreateAssetHandle(Shader::Create(shaderDesc)));
    const Ref<Material> material = Material::Create(shader);
    material->SetVariation(toonVariation);
    const AssetHandle<Material> materialHandle = static_asset_cast<Material>(manager.CreateAssetHandle(material));

    GpuScene scene(false);
    const RenderMaterialResourceChange create{ 1, 1, RenderResourceChangeType::CreateOrUpdate, materialHandle };
    scene.ApplyResources(nullptr, 0, &create, 1);
    CHECK(scene.HasToonOutlineMaterials());
    CHECK_FALSE(scene.HasToonSilhouetteMaterials());

    const RenderMaterialResourceChange destroy{ 1, 2, RenderResourceChangeType::Destroy, {} };
    scene.ApplyResources(nullptr, 0, &destroy, 1);
    CHECK_FALSE(scene.HasToonOutlineMaterials());
    CHECK_FALSE(scene.HasToonSilhouetteMaterials());
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

TEST_CASE("Decal textures reuse the shared table and report admission failures", "[Renderer][GpuScene][Bindless][decals]")
{
    ScopedSceneTextures defaults;
    GpuScene scene(false, 7);
    const auto texture = CreateRef<SceneTestTexture>();
    Vector<uint32_t> indices;
    scene.SetDecalTextures({ texture, texture }, indices);
    REQUIRE(indices.size() == 2);
    REQUIRE(indices[0] != UINT32_MAX);
    CHECK(indices[0] == indices[1]);
    const auto version = scene.GetBindlessTextureVersion();
    const auto retained = indices[0];
    scene.SetDecalTextures({ texture }, indices);
    CHECK(indices[0] == retained);
    CHECK(scene.GetBindlessTextureVersion() == version);
    scene.SetDecalTextures({ texture, CreateRef<SceneTestTexture>(), CreateRef<SceneTestTexture>(), CreateRef<SceneTestTexture>() }, indices);
    CHECK(std::count(indices.begin(), indices.end(), UINT32_MAX) == 1);
    scene.SetDecalTextures({}, indices);
    CHECK(indices.empty());
    CHECK(scene.GetBindlessTextures()[retained] == Texture::MISSING);
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

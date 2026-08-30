#include <catch2/catch_test_macros.hpp>

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Renderer/GpuMaterial.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Serialization/MaterialSerializer.h"

using namespace Crowny;

TEST_CASE("GPU standard materials pack a bounded stable record", "[Renderer][Materials]")
{
    StandardMaterialDesc desc;
    desc.BaseColor = { -1.0f, 0.5f, 2.0f, 0.25f };
    desc.Emissive = { 1.0f, 2.0f, 3.0f };
    desc.EmissiveIntensity = 4.0f;
    desc.AlphaCutoff = 2.0f;
    desc.Metallic = -1.0f;
    desc.Roughness = 0.0f;
    desc.AmbientOcclusion = 2.0f;
    desc.BaseColorTexture = 7;
    desc.Alpha = AlphaMode::Mask;
    desc.Flags = GpuMaterialFlags::TwoSided | GpuMaterialFlags::ReceiveShadows;

    const GpuMaterialData packed = GpuMaterialPacker::Pack(desc);
    CHECK(packed.BaseColor.r == 0.0f);
    CHECK(packed.BaseColor.g == 0.5f);
    CHECK(packed.BaseColor.b == 2.0f);
    CHECK(packed.EmissiveAlphaCutoff == glm::vec4(4.0f, 8.0f, 12.0f, 1.0f));
    CHECK(packed.MetallicRoughnessNormalAo.x == 0.0f);
    CHECK(packed.MetallicRoughnessNormalAo.y == 0.045f);
    CHECK(packed.MetallicRoughnessNormalAo.w == 1.0f);
    CHECK(packed.TextureIndices0.x == 7);
    CHECK(packed.TextureIndices1.z == static_cast<uint32_t>(desc.Flags));
    CHECK(packed.TextureIndices1.w == GpuMaterialPacker::PackModelAndAlpha(MaterialModel::Standard, AlphaMode::Mask));
}

TEST_CASE("GPU toon materials pack bounded artistic controls", "[Renderer][Materials][Toon]")
{
    StandardMaterialDesc desc;
    desc.Model = MaterialModel::Toon;
    desc.ToonBands = 100.0f;
    desc.ToonBandSmoothness = -1.0f;
    desc.ToonSpecularThreshold = 2.0f;
    desc.ToonRimPower = 0.0f;
    desc.ToonRimShadowMask = 2.0f;
    desc.ToonPatternTexture = 42;
    desc.ToonRampTexture = 43;
    desc.ToonMatcapTexture = 44;
    desc.ToonPatternMappingMode = ToonPatternMapping::Triplanar;
    desc.ToonRampStrength = 2.0f;
    desc.ToonRampOffset = -2.0f;
    desc.ToonMatcapStrength = -1.0f;
    desc.ToonOutlineWidth = -2.0f;

    const GpuMaterialData packed = GpuMaterialPacker::Pack(desc);
    CHECK((packed.TextureIndices1.w & 0xffu) == static_cast<uint32_t>(MaterialModel::Toon));
    CHECK(packed.ToonShadowBands.w == 16.0f);
    CHECK(packed.ToonControls.x == 0.0f);
    CHECK(packed.ToonSpecular.w == 1.0f);
    CHECK(packed.ToonArtistic.x == 0.01f);
    CHECK(packed.ToonArtistic.z == 1.0f);
    CHECK(packed.TextureIndices2.x == 42);
    CHECK(packed.TextureIndices2.y == 43);
    CHECK(packed.TextureIndices2.z == 44);
    CHECK(packed.TextureIndices2.w == static_cast<uint32_t>(ToonPatternMapping::Triplanar));
    CHECK(packed.ToonStyle.x == 1.0f);
    CHECK(packed.ToonStyle.y == -1.0f);
    CHECK(packed.ToonStyle.z == 0.0f);
    CHECK(packed.ToonOutline.x == 0.0f);
}

TEST_CASE("Material render classification is explicit and fails closed", "[Renderer][Materials][Routing]")
{
    const MaterialRenderClassification standard =
      MaterialRenderClassifier::Classify("Anything.glsl", { "material_model=standard" }, false, false);
    CHECK(standard.UsesStandardGpuRecord());
    CHECK(standard.Model == MaterialModel::Standard);
    CHECK(standard.Alpha == AlphaMode::Opaque);

    const MaterialRenderClassification unlit =
      MaterialRenderClassifier::Classify("Anything.glsl", { "material_model=unlit" }, true, false);
    CHECK(unlit.UsesStandardGpuRecord());
    CHECK(unlit.Model == MaterialModel::Unlit);
    CHECK(unlit.Alpha == AlphaMode::Premultiplied);

    const MaterialRenderClassification toon =
      MaterialRenderClassifier::Classify("Anything.glsl", { "material_model=toon" }, true, true);
    CHECK(toon.UsesStandardGpuRecord());
    CHECK(toon.Model == MaterialModel::Toon);
    CHECK(toon.Alpha == AlphaMode::Mask);

    const MaterialRenderClassification custom =
      MaterialRenderClassifier::Classify("Pbribl.glsl", { "material_model=custom" }, false, false);
    CHECK_FALSE(custom.UsesStandardGpuRecord());
    CHECK(custom.IsForwardOnlyOpaque());

    const MaterialRenderClassification customMask =
      MaterialRenderClassifier::Classify("Anything.glsl", { "material_model=custom" }, false, true);
    CHECK(customMask.Alpha == AlphaMode::Mask);
    CHECK_FALSE(customMask.IsForwardOnlyOpaque());

    const MaterialRenderClassification customTransparent =
      MaterialRenderClassifier::Classify("Anything.glsl", { "material_model=custom" }, true, false);
    CHECK(customTransparent.Alpha == AlphaMode::Premultiplied);
    CHECK_FALSE(customTransparent.IsForwardOnlyOpaque());
}

TEST_CASE("Only exact built-in shader names use the compatibility material route", "[Renderer][Materials][Routing]")
{
    CHECK(MaterialRenderClassifier::Classify("Resources/Shaders/Pbribl.asset", {}, false, false).Model == MaterialModel::Standard);
    CHECK(MaterialRenderClassifier::Classify("Unlit.glsl", {}, false, false).Model == MaterialModel::Unlit);
    CHECK(MaterialRenderClassifier::Classify("Toon.asset", {}, false, false).Model == MaterialModel::Toon);

    const MaterialRenderClassification lookalike = MaterialRenderClassifier::Classify("MyToonShader.asset", {}, false, false);
    CHECK_FALSE(lookalike.UsesStandardGpuRecord());
    CHECK_FALSE(lookalike.IsForwardOnlyOpaque());
    CHECK(lookalike.IsUnsupported());

    const GpuMaterialData unsupported = GpuMaterialPacker::PackUnsupported();
    CHECK(unsupported.TextureIndices1.w == GpuMaterialPacker::UnsupportedModelAndAlpha);
    CHECK(((unsupported.TextureIndices1.w >> 8u) & 0xffu) > static_cast<uint32_t>(AlphaMode::WeightedOIT));
}

TEST_CASE("Materials can explicitly request and persist weighted OIT routing", "[Renderer][Materials][Routing]")
{
    const Path assetPath = fs::temp_directory_path() / "crowny-weighted-oit-material.asset";
    fs::remove(assetPath);

    struct ScopedAssetManager
    {
        ScopedAssetManager()
        {
            AssetListenerManager::StartUp();
            AssetManager::StartUp();
        }
        ~ScopedAssetManager()
        {
            AssetManager::Shutdown();
            AssetListenerManager::Shutdown();
        }
    } scopedAssetManager;
    AssetManager& manager = AssetManager::Get();
    const Ref<ShaderTechnique> technique = ShaderTechnique::Create({ "material_model=standard" }, {}, {});
    ShaderDesc shaderDesc;
    shaderDesc.Techniques = { technique };
    const AssetHandle<Shader> shader = static_asset_cast<Shader>(manager.CreateAssetHandle(Shader::Create(shaderDesc)));
    const Ref<Material> material = Material::Create(shader);

    CHECK_FALSE(material->HasAlphaModeOverride());
    CHECK(MaterialRenderClassifier::Classify(*material).Alpha == AlphaMode::Opaque);
    const uint64_t initialVersion = material->GetParamVersion();

    material->SetAlphaMode(AlphaMode::WeightedOIT);
    CHECK(material->HasAlphaModeOverride());
    CHECK(material->GetAlphaMode() == AlphaMode::WeightedOIT);
    CHECK(material->GetParamVersion() == initialVersion + 1u);
    CHECK(MaterialRenderClassifier::Classify(*material).Alpha == AlphaMode::WeightedOIT);
    material->SetAlphaMode(AlphaMode::WeightedOIT);
    CHECK(material->GetParamVersion() == initialVersion + 1u);

    manager.Save(material, assetPath);
    const AssetHandle<Material> restored = manager.Load<Material>(assetPath, false);
    REQUIRE(restored);
    CHECK(restored->HasAlphaModeOverride());
    CHECK(restored->GetAlphaMode() == AlphaMode::WeightedOIT);
    CHECK(MaterialRenderClassifier::Classify(*restored).Alpha == AlphaMode::WeightedOIT);

    MaterialSerializer serializer(material);
    const String yaml = serializer.SerializeToString();
    CHECK(yaml.find("AlphaMode: WeightedOIT") != String::npos);

    material->ClearAlphaModeOverride();
    REQUIRE(serializer.DeserializeFromString("Version: 2\nAlphaMode: WeightedOIT\n"));
    CHECK(material->HasAlphaModeOverride());
    CHECK(material->GetAlphaMode() == AlphaMode::WeightedOIT);
    CHECK_FALSE(serializer.DeserializeFromString("Version: 2\nAlphaMode: Sorted\n"));
    CHECK_FALSE(serializer.DeserializeFromString("Version: 999\nAlphaMode: Opaque\n"));

    REQUIRE(serializer.DeserializeFromString("Version: 1\n"));
    CHECK_FALSE(material->HasAlphaModeOverride());
    CHECK(MaterialRenderClassifier::Classify(*material).Alpha == AlphaMode::Opaque);

    fs::remove(assetPath);
}

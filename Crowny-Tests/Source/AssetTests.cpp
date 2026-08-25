#include <catch2/catch_test_macros.hpp>

#include "Crowny/Assets/AssetCodecs.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Assets/AssetManifest.h"
#include "Crowny/Physics/PhysicsMaterial.h"
#include "Crowny/RenderAPI/GraphicsPipeline.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/Serialization/AssetManifestSerializer.h"
#include "Crowny/Serialization/PhysicsMaterial2DSerializer.h"
#include "cwpch.h"

using namespace Crowny;

class MockAsset : public Asset
{
public:
    MockAsset() = default;
    virtual AssetType GetAssetType() const override { return AssetType::Mesh; } // Just use a dummy type
    static AssetType GetStaticType() { return AssetType::Mesh; }
};

TEST_CASE("Asset Handling", "[Assets]")
{
    AssetManager manager;

    SECTION("Manual Handle Creation")
    {
        Ref<MockAsset> asset = CreateRef<MockAsset>();
        UUID uuid = UuidGenerator::Generate();

        AssetHandle<MockAsset> handle = static_asset_cast<MockAsset>(manager.CreateAssetHandle(asset, uuid));

        CHECK(handle.IsLoaded());
        CHECK(handle.GetUUID() == uuid);
        CHECK(handle.Get() == asset.get());
    }

    SECTION("Reference Counting")
    {
        Ref<MockAsset> asset = CreateRef<MockAsset>();
        AssetHandle<MockAsset> handle1 = static_asset_cast<MockAsset>(manager.CreateAssetHandle(asset));

        {
            AssetHandle<MockAsset> handle2 = handle1;
            // GetRefCount not available on handle itself, it's in handle data
            CHECK(handle1.GetHandleData()->m_RefCount.load() == 2);
        }

        CHECK(handle1.GetHandleData()->m_RefCount.load() == 1);
    }

    SECTION("Weak Handles")
    {
        AssetHandle<MockAsset> handle = static_asset_cast<MockAsset>(manager.CreateAssetHandle(CreateRef<MockAsset>()));
        WeakAssetHandle<MockAsset> weak = handle.GetWeak();

        CHECK_FALSE(weak.IsExpired());
        CHECK(weak.IsLoaded());

        handle = nullptr; // Release strong reference
        CHECK(weak.IsExpired());
        CHECK_FALSE(weak.IsLoaded());
    }
}

TEST_CASE("Asset manifests preserve a UUID-path bijection", "[Assets][Manifest]")
{
    AssetManifest manifest("Tests");
    const UUID first(1, 2, 3, 4);
    const UUID second(5, 6, 7, 8);

    manifest.RegisterAsset(first, Path("Assets/Textures/../Textures/grid.png"));
    CHECK(manifest.FilepathExists(Path("Assets/Textures/grid.png")));

    UUID resolved;
    REQUIRE(manifest.FilepathToUuid(Path("Assets/Textures/grid.png"), resolved));
    CHECK(resolved == first);

    manifest.RegisterAsset(second, Path("Assets/Textures/grid.png"));
    CHECK_FALSE(manifest.UuidExists(first));
    CHECK(manifest.UuidExists(second));

    manifest.RegisterAsset(second, Path("Assets/Textures/grid-renamed.png"));
    CHECK_FALSE(manifest.FilepathExists(Path("Assets/Textures/grid.png")));
    REQUIRE(manifest.FilepathToUuid(Path("Assets/Textures/grid-renamed.png"), resolved));
    CHECK(resolved == second);
}

TEST_CASE("Asset manifest YAML rejects unsupported schemas and resolves collisions", "[Assets][Manifest][Serialization]")
{
    YAML::Node future = YAML::Load("Version: 999\nManifest: Future\nAssets: []\n");
    CHECK(AssetManifestSerializer::Deserialize(future) == nullptr);

    YAML::Node duplicatePath = YAML::Load("Version: 1\nManifest: Tests\nAssets:\n  - 00000001-0000-0002-0000-000300000004: Assets/a.asset\n"
                                          "  - 00000005-0000-0006-0000-000700000008: Assets/a.asset\n");
    Ref<AssetManifest> manifest = AssetManifestSerializer::Deserialize(duplicatePath);
    REQUIRE(manifest != nullptr);

    const UUID first(1, 2, 3, 4);
    const UUID second(5, 6, 7, 8);
    CHECK_FALSE(manifest->UuidExists(first));
    CHECK(manifest->UuidExists(second));
}

TEST_CASE("Physics material YAML preserves values and supplies legacy defaults", "[Assets][Physics][Serialization]")
{
    Ref<PhysicsMaterial3D> source = CreateRef<PhysicsMaterial3D>();
    source->SetDensity(2.25f);
    source->SetFriction(0.35f);
    source->SetRestitution(0.7f);
    source->SetRestitutionThreshold(1.5f);
    source->SetFrictionCombine(PhysicsCombineMode::Multiply);
    source->SetRestitutionCombine(PhysicsCombineMode::Minimum);

    YAML::Emitter emitter;
    PhysicsMaterial3DSerializer::Serialize(source, emitter);
    const Ref<PhysicsMaterial3D> restored = PhysicsMaterial3DSerializer::Deserialize(YAML::Load(emitter.c_str()));
    REQUIRE(restored != nullptr);
    CHECK(restored->GetDensity() == 2.25f);
    CHECK(restored->GetFriction() == 0.35f);
    CHECK(restored->GetRestitution() == 0.7f);
    CHECK(restored->GetRestitutionThreshold() == 1.5f);
    CHECK(restored->GetFrictionCombine() == PhysicsCombineMode::Multiply);
    CHECK(restored->GetRestitutionCombine() == PhysicsCombineMode::Minimum);

    const Ref<PhysicsMaterial2D> legacy = PhysicsMaterial2DSerializer::Deserialize(YAML::Load("Friction: 0.2\nUnknown: ignored\n"));
    REQUIRE(legacy != nullptr);
    CHECK(legacy->GetDensity() == 1.0f);
    CHECK(legacy->GetFriction() == 0.2f);
    CHECK(legacy->GetRestitution() == 0.0f);
    CHECK(legacy->GetRestitutionThreshold() == 0.5f);
    CHECK(legacy->GetFrictionCombine() == PhysicsCombineMode::GeometricMean);
    CHECK(legacy->GetRestitutionCombine() == PhysicsCombineMode::Maximum);

    CHECK(PhysicsMaterial2DSerializer::Deserialize(YAML::Load("Version: 999\n")) == nullptr);
    CHECK(PhysicsMaterial3DSerializer::Deserialize(YAML::Load("Density: invalid\n")) == nullptr);
}

TEST_CASE("Physics material assets survive binary round trips", "[Assets][Physics][Serialization]")
{
    const Path material2DPath = fs::temp_directory_path() / "crowny-physics-material-roundtrip.pmat";
    const Path material3DPath = fs::temp_directory_path() / "crowny-physics-material-roundtrip.pmat3d";
    fs::remove(material2DPath);
    fs::remove(material3DPath);

    AssetManager manager;
    Ref<PhysicsMaterial2D> material2D = CreateRef<PhysicsMaterial2D>();
    material2D->SetDensity(3.0f);
    material2D->SetFriction(0.6f);
    material2D->SetRestitution(0.4f);
    material2D->SetRestitutionThreshold(2.0f);
    material2D->SetFrictionCombine(PhysicsCombineMode::Maximum);
    material2D->SetRestitutionCombine(PhysicsCombineMode::Average);
    manager.Save(material2D, material2DPath);

    Ref<PhysicsMaterial3D> material3D = CreateRef<PhysicsMaterial3D>();
    material3D->SetDensity(4.0f);
    material3D->SetFriction(0.3f);
    material3D->SetRestitution(0.8f);
    material3D->SetRestitutionThreshold(0.75f);
    material3D->SetFrictionCombine(PhysicsCombineMode::Minimum);
    material3D->SetRestitutionCombine(PhysicsCombineMode::Multiply);
    manager.Save(material3D, material3DPath);

    const AssetHandle<PhysicsMaterial2D> loaded2D = manager.Load<PhysicsMaterial2D>(material2DPath, false);
    const AssetHandle<PhysicsMaterial3D> loaded3D = manager.Load<PhysicsMaterial3D>(material3DPath, false);
    REQUIRE(loaded2D);
    REQUIRE(loaded3D);
    CHECK(loaded2D->GetDensity() == 3.0f);
    CHECK(loaded2D->GetFrictionCombine() == PhysicsCombineMode::Maximum);
    CHECK(loaded2D->GetRestitutionCombine() == PhysicsCombineMode::Average);
    CHECK(loaded3D->GetDensity() == 4.0f);
    CHECK(loaded3D->GetRestitutionThreshold() == 0.75f);
    CHECK(loaded3D->GetFrictionCombine() == PhysicsCombineMode::Minimum);
    CHECK(loaded3D->GetRestitutionCombine() == PhysicsCombineMode::Multiply);

    fs::remove(material2DPath);
    fs::remove(material3DPath);
}

TEST_CASE("Shader state descriptors survive asset round trips", "[Assets][Shader][Serialization]")
{
    const Path assetPath = fs::temp_directory_path() / "crowny-shader-state-roundtrip.asset";
    fs::remove(assetPath);

    AssetManager manager;
    ShaderRenderPassDesc passDesc;
    passDesc.BlendState = CreateRef<BlendStateDesc>();
    passDesc.BlendState->EnableBlending = true;
    passDesc.BlendState->AlphaToCoverage = true;
    passDesc.BlendState->SrcBlend = BlendFactor::SourceAlpha;
    passDesc.BlendState->DstBlend = BlendFactor::InvSourceAlpha;
    passDesc.BlendState->BlendOp = BlendFunction::REVERSE_SUBTRACT;

    passDesc.RasterizationState = CreateRef<RasterizerStateDesc>();
    passDesc.RasterizationState->CullMode = CullingMode::CULL_COUNTERCLOCKWISE;
    passDesc.RasterizationState->DepthBias = 1.25f;
    passDesc.RasterizationState->DepthBiasSlope = 2.5f;
    passDesc.RasterizationState->DepthBiasClamp = 3.75f;
    passDesc.RasterizationState->PolygonDrawMode = PolygonMode::Wireframe;
    passDesc.RasterizationState->DepthClipEnable = true;
    passDesc.RasterizationState->ScissorsEnabled = true;

    passDesc.DepthStencilState = CreateRef<DepthStencilStateDesc>();
    passDesc.DepthStencilState->EnableDepthRead = false;
    passDesc.DepthStencilState->EnableDepthWrite = false;
    passDesc.DepthStencilState->DepthCompareFunction = CompareFunction::GREATER_EQUAL;
    passDesc.DepthStencilState->EnableStencil = true;
    passDesc.DepthStencilState->StencilReadMask = 0x3c;
    passDesc.DepthStencilState->StencilWriteMask = 0xc3;
    passDesc.DepthStencilState->StencilFrontCompare = CompareFunction::NOT_EQUAL;
    passDesc.DepthStencilState->StencilFrontFailOp = StencilOperation::Replace;
    passDesc.DepthStencilState->StencilBackPassOp = StencilOperation::Invert;

    Ref<ShaderRenderPass> pass = ShaderRenderPass::Create(passDesc);
    Ref<ShaderTechnique> technique = ShaderTechnique::Create({}, {}, { pass });
    ShaderDesc shaderDesc;
    shaderDesc.Techniques = { technique };
    Ref<Shader> shader = Shader::Create(shaderDesc);
    shader->SetSourceTimestamp(123456789);
    shader->SetSourceContentHash(0x123456789abcdef0ull);
    manager.Save(shader, assetPath);

    AssetFileHeader header;
    REQUIRE(PeekAssetHeader(assetPath, header));
    CHECK(header.Type == AssetType::Shader);
    CHECK(header.Version == SHADER_FORMAT_VERSION);
    CHECK(header.SourceTimestamp == 123456789);
    CHECK(header.SourceContentHash == 0x123456789abcdef0ull);

    passDesc = {};
    pass = nullptr;
    technique = nullptr;
    shader = nullptr;

    AssetHandle<Shader> loaded = manager.Load<Shader>(assetPath, false);
    REQUIRE(loaded);
    REQUIRE(loaded->GetTechniques().size() == 1);
    REQUIRE(loaded->GetTechniques()[0]->GetRenderPasses().size() == 1);
    const ShaderRenderPassDesc& loadedDesc = loaded->GetTechniques()[0]->GetRenderPasses()[0]->GetPassDesc();
    REQUIRE(loadedDesc.BlendState);
    REQUIRE(loadedDesc.RasterizationState);
    REQUIRE(loadedDesc.DepthStencilState);

    CHECK(loadedDesc.BlendState->EnableBlending);
    CHECK(loadedDesc.BlendState->AlphaToCoverage);
    CHECK(loadedDesc.BlendState->SrcBlend == BlendFactor::SourceAlpha);
    CHECK(loadedDesc.BlendState->DstBlend == BlendFactor::InvSourceAlpha);
    CHECK(loadedDesc.BlendState->BlendOp == BlendFunction::REVERSE_SUBTRACT);
    CHECK(loadedDesc.BlendState->GetRefCount() == 1);

    CHECK(loadedDesc.RasterizationState->CullMode == CullingMode::CULL_COUNTERCLOCKWISE);
    CHECK(loadedDesc.RasterizationState->DepthBias == 1.25f);
    CHECK(loadedDesc.RasterizationState->DepthBiasSlope == 2.5f);
    CHECK(loadedDesc.RasterizationState->DepthBiasClamp == 3.75f);
    CHECK(loadedDesc.RasterizationState->PolygonDrawMode == PolygonMode::Wireframe);
    CHECK(loadedDesc.RasterizationState->DepthClipEnable);
    CHECK(loadedDesc.RasterizationState->ScissorsEnabled);
    CHECK(loadedDesc.RasterizationState->GetRefCount() == 1);

    CHECK_FALSE(loadedDesc.DepthStencilState->EnableDepthRead);
    CHECK_FALSE(loadedDesc.DepthStencilState->EnableDepthWrite);
    CHECK(loadedDesc.DepthStencilState->DepthCompareFunction == CompareFunction::GREATER_EQUAL);
    CHECK(loadedDesc.DepthStencilState->EnableStencil);
    CHECK(loadedDesc.DepthStencilState->StencilReadMask == 0x3c);
    CHECK(loadedDesc.DepthStencilState->StencilWriteMask == 0xc3);
    CHECK(loadedDesc.DepthStencilState->StencilFrontCompare == CompareFunction::NOT_EQUAL);
    CHECK(loadedDesc.DepthStencilState->StencilFrontFailOp == StencilOperation::Replace);
    CHECK(loadedDesc.DepthStencilState->StencilBackPassOp == StencilOperation::Invert);
    CHECK(loadedDesc.DepthStencilState->GetRefCount() == 1);

    fs::remove(assetPath);
}

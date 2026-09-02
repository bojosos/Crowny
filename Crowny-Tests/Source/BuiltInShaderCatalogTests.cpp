#include <catch2/catch_test_macros.hpp>

#include "Crowny/Assets/AssetCodecs.h"
#include "Crowny/Assets/AssetListener.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Assets/AssetManifest.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/Renderer/BuiltInShaderCatalog.h"
#include "Panels/MaterialInspectorSchemaCache.h"

#include <filesystem>

using namespace Crowny;

namespace
{
    struct ScopedAssetManager
    {
        ScopedAssetManager()
        {
            if (AssetListenerManager::TryGet() == nullptr)
            {
                AssetListenerManager::StartUp();
                OwnsListenerManager = true;
            }
            if (AssetManager::TryGet() == nullptr)
            {
                AssetManager::StartUp();
                OwnsAssetManager = true;
            }
        }
        ~ScopedAssetManager()
        {
            if (OwnsAssetManager)
                AssetManager::Shutdown();
            if (OwnsListenerManager)
                AssetListenerManager::Shutdown();
        }
        bool OwnsListenerManager = false;
        bool OwnsAssetManager = false;
    };

    Ref<Shader> MakeShader(const Vector<String>& tags, bool compute = false)
    {
        ShaderRenderPassDesc passDesc;
        passDesc.BlendState = CreateRef<BlendStateDesc>();
        passDesc.RasterizationState = CreateRef<RasterizerStateDesc>();
        passDesc.DepthStencilState = CreateRef<DepthStencilStateDesc>();
        if (compute)
            passDesc.ComputeShader = CreateRef<BinaryShaderData>(Vector<uint8_t>{ 1, 2, 3 }, "main", COMPUTE_SHADER, nullptr);
        Ref<ShaderRenderPass> pass = ShaderRenderPass::Create(passDesc);
        ShaderDesc desc;
        desc.Techniques = { ShaderTechnique::Create(tags, {}, { pass }) };
        return Shader::Create(desc);
    }
} // namespace

TEST_CASE("Built-in shader identifiers are stable and unique", "[Renderer][Shader][Catalog]")
{
    const UUID unlit = BuiltInShaderCatalog::MakeStableUuid("Resources/Shaders/Unlit.asset");
    CHECK(unlit == BuiltInShaderCatalog::MakeStableUuid("Resources/Shaders/Unlit.asset"));
    CHECK_FALSE(unlit.Empty());
    CHECK(unlit != BuiltInShaderCatalog::MakeStableUuid("Resources/Shaders/Toon.asset"));
    CHECK(unlit != BuiltInShaderCatalog::MakeStableUuid("Resources/Shaders/unlit.asset"));
    // Name-based identifiers carry the version 8 marker so they can never collide with random version 4 ids.
    CHECK(unlit.ToString()[14] == '8');
}

TEST_CASE("Built-in shader catalog lists shader assets from a directory", "[Renderer][Shader][Catalog]")
{
    ScopedAssetManager scopedAssetManager;
    const Path root = fs::temp_directory_path() / "crowny-builtin-shader-catalog";
    std::error_code error;
    fs::remove_all(root, error);
    REQUIRE(fs::create_directories(root));

    AssetManager& manager = AssetManager::Get();
    REQUIRE(manager.Save(MakeShader({ "material_model=custom" }), root / "Zebra.asset"));
    REQUIRE(manager.Save(MakeShader({}), root / "Alpha.asset"));
    REQUIRE(FileSystem::WriteTextFile(root / "Notes.asset", "not an asset"));
    REQUIRE(FileSystem::WriteTextFile(root / "Alpha.glsl", "#lang glsl"));

    const Vector<BuiltInShaderEntry> entries = BuiltInShaderCatalog::EnumerateDirectory(root);
    REQUIRE(entries.size() == 2u);
    CHECK(entries[0].Name == "Alpha");
    CHECK(entries[0].AssetPath.generic_string() == "Resources/Shaders/Alpha.asset");
    CHECK(entries[0].Uuid == BuiltInShaderCatalog::MakeStableUuid("Resources/Shaders/Alpha.asset"));
    CHECK(entries[1].Name == "Zebra");

    const Ref<AssetManifest> manifest = BuiltInShaderCatalog::BuildManifest(entries);
    REQUIRE(manifest != nullptr);
    UUID uuid;
    REQUIRE(manifest->FilepathToUuid(Path("Resources/Shaders/Zebra.asset"), uuid));
    CHECK(uuid == entries[1].Uuid);
    Path path;
    REQUIRE(manifest->UuidToFilepath(entries[0].Uuid, path));
    CHECK(path.generic_string() == "Resources/Shaders/Alpha.asset");
    CHECK_FALSE(manifest->UuidExists(BuiltInShaderCatalog::MakeStableUuid("Resources/Shaders/Notes.asset")));

    fs::remove_all(root, error);
}

TEST_CASE("Built-in shader catalog classifies material shaders", "[Renderer][Shader][Catalog]")
{
    CHECK(BuiltInShaderCatalog::IsMaterialShader("Resources/Shaders/Anything.asset", *MakeShader({ "material_model=custom" })));
    CHECK(BuiltInShaderCatalog::IsMaterialShader("Resources/Shaders/Custom.asset", *MakeShader({ "material_model=toon" })));
    CHECK(BuiltInShaderCatalog::IsMaterialShader("Resources/Shaders/Unlit.asset", *MakeShader({})));
    CHECK(BuiltInShaderCatalog::IsMaterialShader("Resources/Shaders/Pbribl.asset", *MakeShader({})));
    CHECK_FALSE(BuiltInShaderCatalog::IsMaterialShader("Resources/Shaders/Bloom.asset", *MakeShader({})));
    CHECK_FALSE(BuiltInShaderCatalog::IsMaterialShader("Resources/Shaders/Unlit.asset", *MakeShader({}, true)));
    CHECK_FALSE(BuiltInShaderCatalog::IsMaterialShader("Resources/Shaders/Unlit.asset", *Shader::Create(ShaderDesc{})));
}

TEST_CASE("Material shader picker filters and orders options", "[Editor][Material][Shader]")
{
    Vector<MaterialShaderOption> options;
    const auto add = [&options](const char* name, bool builtIn, bool capable) {
        MaterialShaderOption option;
        option.Name = name;
        option.Uuid = BuiltInShaderCatalog::MakeStableUuid(name);
        option.BuiltIn = builtIn;
        option.MaterialCapable = capable;
        options.push_back(std::move(option));
    };
    add("Water", false, true);
    add("Unlit", true, true);
    add("GpuCullInstances", true, false);
    add("Toon", true, true);
    add("Bark", false, true);

    Vector<const MaterialShaderOption*> filtered = FilterMaterialShaderOptions(options, "", false);
    REQUIRE(filtered.size() == 4u);
    CHECK(filtered[0]->Name == "Toon");
    CHECK(filtered[1]->Name == "Unlit");
    CHECK(filtered[2]->Name == "Bark");
    CHECK(filtered[3]->Name == "Water");

    filtered = FilterMaterialShaderOptions(options, "", true);
    REQUIRE(filtered.size() == 5u);
    CHECK(filtered[2]->Name == "GpuCullInstances");

    filtered = FilterMaterialShaderOptions(options, "cull", false);
    CHECK(filtered.empty());
    filtered = FilterMaterialShaderOptions(options, "cull", true);
    REQUIRE(filtered.size() == 1u);
    CHECK(filtered[0]->Name == "GpuCullInstances");

    filtered = FilterMaterialShaderOptions(options, "T", false);
    REQUIRE(filtered.size() == 4u);
    CHECK(filtered[0]->Name == "Toon");
}

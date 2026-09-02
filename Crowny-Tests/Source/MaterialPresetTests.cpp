#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Crowny/Assets/AssetCodecs.h"
#include "Crowny/Assets/AssetListener.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/MaterialPreset.h"
#include "Crowny/Renderer/MaterialPresetLibrary.h"
#include "Crowny/Serialization/MaterialPresetSerializer.h"

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

    Material::UniformMember Member(ShaderDataType type)
    {
        Material::UniformMember member{};
        member.DataType = type;
        member.BufferName = "Params";
        return member;
    }

    Material::BindingMap ToonBindings()
    {
        Material::BindingMap bindings;
        for (const char* name : { "thickness", "toonSilhouetteWidth", "bands", "specularSize", "specularSmoothness", "shadowBrightness",
                                  "toonBandSmoothness", "toonSpecularThreshold", "toonSpecularSmoothness", "toonSpecularStrength", "rimPower",
                                  "rimThreshold", "toonRimSmoothness", "toonRimStrength", "toonRimShadowMask", "toonIndirectStrength",
                                  "toonPatternScale", "toonPatternStrength", "toonPatternSmoothness", "toonPatternDistanceFade",
                                  "toonRampStrength", "toonRampOffset", "toonMatcapStrength", "toonMatcapRotation", "toonOutlineDepthThreshold",
                                  "toonOutlineNormalThreshold", "toonOutlineDistanceFade" })
            bindings.emplace(name, Member(ShaderDataType::Float));
        for (const char* name : { "outlineColor", "tint", "toonShadowColor", "toonSpecularColor", "toonRimColor" })
            bindings.emplace(name, Member(ShaderDataType::Float4));
        bindings.emplace("toonPatternMapping", Member(ShaderDataType::Int));
        return bindings;
    }
} // namespace

TEST_CASE("Material presets round trip through YAML", "[Renderer][Material][Preset]")
{
    Ref<MaterialPreset> preset = CreateRef<MaterialPreset>();
    preset->SetName("Ink");
    preset->SetTarget("Toon");
    preset->SetColor("tint", glm::vec4(0.5f, 0.25f, 1.0f, 1.0f));
    preset->SetFloat("bands", 3.0f);
    preset->SetFloat2("uvScale", glm::vec2(2.0f, 4.0f));
    preset->SetFloat3("offset", glm::vec3(1.0f, 2.0f, 3.0f));
    preset->SetInt("toonPatternMapping", 3);
    preset->SetBool("useRamp", true);
    preset->SetFloat("bands", 5.0f); // Upsert keeps a single entry
    REQUIRE(preset->GetParameters().size() == 6u);

    const String yaml = MaterialPresetSerializer(preset).SerializeToString();
    CHECK(yaml.find("Target: Toon") != String::npos);
    CHECK(yaml.find("Type: Color") != String::npos);

    Ref<MaterialPreset> restored = CreateRef<MaterialPreset>();
    REQUIRE(MaterialPresetSerializer(restored).DeserializeFromString(yaml));
    CHECK(restored->GetName() == "Ink");
    CHECK(restored->GetTarget() == "Toon");
    REQUIRE(restored->GetParameters().size() == 6u);
    const MaterialPresetParameter* tint = restored->Find("tint");
    REQUIRE(tint != nullptr);
    CHECK(tint->Type == MaterialPresetValueType::Color);
    CHECK(tint->Vector.y == Catch::Approx(0.25f));
    const MaterialPresetParameter* bands = restored->Find("bands");
    REQUIRE(bands != nullptr);
    CHECK(bands->Vector.x == Catch::Approx(5.0f));
    REQUIRE(restored->Find("uvScale") != nullptr);
    CHECK(restored->Find("uvScale")->Vector.y == Catch::Approx(4.0f));
    REQUIRE(restored->Find("offset") != nullptr);
    CHECK(restored->Find("offset")->Vector.z == Catch::Approx(3.0f));
    REQUIRE(restored->Find("toonPatternMapping") != nullptr);
    CHECK(restored->Find("toonPatternMapping")->Integer == 3);
    REQUIRE(restored->Find("useRamp") != nullptr);
    CHECK(restored->Find("useRamp")->Integer == 1);

    // Hand-authored flow style and type aliases are accepted; broken input is rejected.
    Ref<MaterialPreset> authored = CreateRef<MaterialPreset>();
    REQUIRE(MaterialPresetSerializer(authored).DeserializeFromString(
      "Version: 1\nTarget: toon\nParameters:\n  - {Name: tint, Type: Float4, Value: [1, 1, 1, 1]}\n  - {Name: bands, Type: float, Value: 2}\n"));
    CHECK(authored->Find("tint")->Type == MaterialPresetValueType::Color);
    CHECK(authored->Find("bands")->Vector.x == Catch::Approx(2.0f));
    CHECK_FALSE(MaterialPresetSerializer(authored).DeserializeFromString("Version: 99\n"));
    CHECK_FALSE(MaterialPresetSerializer(authored).DeserializeFromString("Version: 1\nParameters:\n  - {Name: x, Type: Matrix, Value: 0}\n"));
    CHECK_FALSE(MaterialPresetSerializer(authored).DeserializeFromString("Version: 1\nParameters:\n  - {Name: x, Type: Float, Value: [1, 2]}\n"));
}

TEST_CASE("Material presets survive binary asset round trips", "[Renderer][Material][Preset][Serialization]")
{
    ScopedAssetManager scopedAssetManager;
    const Path assetPath = fs::temp_directory_path() / "crowny-material-preset-roundtrip.asset";
    fs::remove(assetPath);

    Ref<MaterialPreset> preset = CreateRef<MaterialPreset>();
    preset->SetName("Binary");
    preset->SetTarget("Standard");
    preset->SetColor("albedo", glm::vec4(0.1f, 0.2f, 0.3f, 1.0f));
    preset->SetFloat("roughness", 0.75f);
    preset->SetBool("useIBL", false);

    AssetManager& manager = AssetManager::Get();
    REQUIRE(manager.Save(preset, assetPath));
    AssetFileHeader header;
    REQUIRE(PeekAssetHeader(assetPath, header));
    CHECK(header.Type == AssetType::MaterialPreset);
    CHECK(header.Version == MATERIAL_PRESET_FORMAT_VERSION);

    const AssetHandle<MaterialPreset> restored = manager.Load<MaterialPreset>(assetPath, false);
    REQUIRE(restored);
    CHECK(restored->GetName() == "Binary");
    CHECK(restored->GetTarget() == "Standard");
    REQUIRE(restored->GetParameters().size() == 3u);
    CHECK(restored->Find("albedo")->Vector.z == Catch::Approx(0.3f));
    CHECK(restored->Find("roughness")->Vector.x == Catch::Approx(0.75f));
    CHECK(restored->Find("useIBL")->Integer == 0);
    fs::remove(assetPath);
}

TEST_CASE("Material presets validate against the shader layout", "[Renderer][Material][Preset]")
{
    Material::BindingMap bindings = ToonBindings();
    Ref<MaterialPreset> preset = CreateRef<MaterialPreset>();
    preset->SetFloat("bands", 3.0f);
    preset->SetColor("tint", glm::vec4(1.0f));
    String error;
    CHECK(preset->Validate(bindings, &error));
    CHECK(error.empty());

    preset->SetFloat("tint", 1.0f); // wrong type
    CHECK_FALSE(preset->Validate(bindings, &error));
    CHECK(error.find("tint") != String::npos);

    preset->SetColor("tint", glm::vec4(1.0f));
    preset->SetFloat("doesNotExist", 1.0f);
    CHECK_FALSE(preset->Validate(bindings, &error));
    CHECK(error.find("doesNotExist") != String::npos);
    CHECK(preset->Remove("doesNotExist"));
    CHECK_FALSE(preset->Remove("doesNotExist"));
    CHECK(preset->Validate(bindings));
}

TEST_CASE("Material presets target a material model or shader", "[Renderer][Material][Preset]")
{
    Ref<MaterialPreset> preset = CreateRef<MaterialPreset>();
    CHECK(preset->IsCompatibleWith(MaterialModel::Standard, "Resources/Shaders/Pbribl.asset"));
    preset->SetTarget("toon");
    CHECK(preset->IsCompatibleWith(MaterialModel::Toon, ""));
    CHECK_FALSE(preset->IsCompatibleWith(MaterialModel::Standard, "Resources/Shaders/Pbribl.asset"));
    preset->SetTarget("MyWater");
    CHECK(preset->IsCompatibleWith(MaterialModel::Standard, "Assets/Shaders/MyWater.glsl"));
    CHECK_FALSE(preset->IsCompatibleWith(MaterialModel::Standard, "Assets/Shaders/Other.glsl"));

    ScopedAssetManager scopedAssetManager;
    const Ref<ShaderTechnique> technique = ShaderTechnique::Create({ "material_model=toon" }, {}, {});
    ShaderDesc shaderDesc;
    shaderDesc.Techniques = { technique };
    const AssetHandle<Shader> shader = static_asset_cast<Shader>(AssetManager::Get().CreateAssetHandle(Shader::Create(shaderDesc)));
    const Ref<Material> material = Material::Create(shader);
    preset->SetTarget("Toon");
    CHECK(preset->IsCompatibleWith(*material));
    preset->SetTarget("Standard");
    CHECK_FALSE(preset->IsCompatibleWith(*material));

    // Without reflected bindings the preset cannot be applied, and the material is left untouched.
    preset->SetTarget("Toon");
    preset->SetFloat("bands", 3.0f);
    CHECK_FALSE(material->ApplyPreset(*preset));
    Ref<MaterialPreset> empty = CreateRef<MaterialPreset>();
    CHECK(material->ApplyPreset(*empty));

    Vector<MaterialPresetEntry> entries;
    MaterialPresetEntry toon;
    toon.Name = "Project/Toon";
    toon.Preset = preset;
    entries.push_back(toon);
    MaterialPresetEntry standard;
    standard.Name = "Project/Standard";
    standard.Preset = CreateRef<MaterialPreset>();
    standard.Preset->SetTarget("Standard");
    entries.push_back(standard);
    const Vector<MaterialPresetEntry> compatible = MaterialPresetLibrary::FilterCompatible(*material, entries);
    REQUIRE(compatible.size() == 1u);
    CHECK(compatible[0].Name == "Project/Toon");
}

TEST_CASE("Built-in toon presets are complete data files", "[Renderer][Material][Preset][Toon]")
{
    const Vector<Path> directories = MaterialPresetLibrary::ResolveBuiltInDirectories();
    REQUIRE_FALSE(directories.empty());
    const Vector<MaterialPresetEntry> builtIn = MaterialPresetLibrary::EnumerateBuiltIn();
    const auto has = [&builtIn](const char* name) {
        return std::any_of(builtIn.begin(), builtIn.end(), [name](const MaterialPresetEntry& entry) { return entry.Name == name; });
    };
    CHECK(has("Toon/Classic"));
    CHECK(has("Toon/Soft"));
    CHECK(has("Toon/Hatched"));

    const Material::BindingMap bindings = ToonBindings();
    for (const ToonMaterialPreset toonPreset : { ToonMaterialPreset::Classic, ToonMaterialPreset::Soft, ToonMaterialPreset::Hatched })
    {
        const String name = MaterialPresetLibrary::BuiltInToonPresetName(toonPreset);
        const Ref<MaterialPreset> preset = MaterialPresetLibrary::LoadBuiltIn(name);
        REQUIRE(preset != nullptr);
        CHECK(preset->GetTarget() == "Toon");
        String error;
        CHECK(preset->Validate(bindings, &error));
        CHECK(error.empty());
        // Every preset pins the full toon look so switching presets never leaves stale values behind.
        CHECK(preset->GetParameters().size() == bindings.size());
        CHECK(MaterialPresetLibrary::LoadBuiltIn(name) == preset); // cached
    }
    CHECK(MaterialPresetLibrary::LoadBuiltIn("Toon/Hatched")->Find("toonPatternMapping")->Integer == 3);
    CHECK(MaterialPresetLibrary::LoadBuiltIn("Toon/Classic")->Find("bands")->Vector.x == Catch::Approx(3.0f));
    CHECK(MaterialPresetLibrary::LoadBuiltIn("Toon/DoesNotExist") == nullptr);
    MaterialPresetLibrary::ClearCache();
}

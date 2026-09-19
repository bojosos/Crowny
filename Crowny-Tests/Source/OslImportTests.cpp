#include "Crowny/Import/ShaderImporter.h"
#include "Crowny/Serialization/ImportOptionsSerializer.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <yaml-cpp/yaml.h>

using namespace Crowny;

TEST_CASE("OSL shader import uses worker scheduling and preserves its output selection", "[OSL][Import]")
{
    OslShaderImporter importer;
    CHECK(importer.IsExtensionSupported("osl"));
    CHECK_FALSE(importer.IsExtensionSupported("glsl"));
    CHECK(importer.GetThreadingPolicy() == ImporterThreadingPolicy::SerializedWorker);
    const auto options = StaticRefCast<ShaderImportOptions>(importer.CreateImportOptions());
    options->SetDefine("CROWNY_OSL_OUTPUT", "alternateColor");
    YAML::Emitter out;
    out << YAML::BeginMap;
    ImportOptionsSerializer::Serialize(out, options);
    out << YAML::EndMap;
    const auto restored = DynamicRefCast<ShaderImportOptions>(ImportOptionsSerializer::Deserialize(YAML::Load(out.c_str())));
    REQUIRE(restored);
    String output;
    CHECK(restored->GetDefine("CROWNY_OSL_OUTPUT", output));
    CHECK(output == "alternateColor");
}

TEST_CASE("Configured OSL importer compiles an editable source into a shader asset", "[OSL][Import][Integration]")
{
    const char* source = std::getenv("CROWNY_TEST_OSL_SOURCE");
    if (!source)
        SKIP("Set CROWNY_TEST_OSL_SOURCE to the editable OSL fixture to exercise the installed compiler.");
    const Path directory = std::filesystem::temp_directory_path() / ("crowny osl test " + UuidGenerator::Generate().ToString());
    std::filesystem::create_directory(directory);
    struct Cleanup
    {
        Path Directory;
        ~Cleanup()
        {
            std::error_code ignored;
            std::filesystem::remove_all(Directory, ignored);
        }
    } cleanup{ directory };
    const Path spacedSource = directory / "Editable OSL texture.osl";
    std::filesystem::copy_file(source, spacedSource);
    OslShaderImporter importer;
    const auto options = StaticRefCast<ShaderImportOptions>(importer.CreateImportOptions());
    const auto asset = importer.Import(spacedSource, options);
    REQUIRE(asset);
    REQUIRE(asset->GetAssetType() == AssetType::Shader);
    const auto shader = StaticRefCast<Shader>(asset);
    REQUIRE(shader->GetTechniques().size() == 1);
    const auto& pass = shader->GetTechniques().front()->GetRenderPasses().front();
    CHECK_FALSE(pass->GetGraphicsPipeline());
    const auto& description = pass->GetPassDesc().FragmentShader->Description;
    REQUIRE(description->Uniforms.contains("OslInputs"));
    CHECK(description->Uniforms.at("OslInputs").Members.size() == 6);
    CHECK(description->Annotations.at("osl_tint").IsColor);
    CHECK(description->Annotations.at("osl_frequency").DisplayName == "Frequency");
    CHECK(description->Annotations.at("osl_frequency").HasRange);
    options->SetDefine("CROWNY_OSL_OUTPUT", "missingOutput");
    CHECK_FALSE(importer.Import(spacedSource, options));
}

TEST_CASE("Configured OSL importer exposes image inputs as texture asset slots", "[OSL][Import][Integration]")
{
    const char* source = std::getenv("CROWNY_TEST_OSL_TEXTURE_SOURCE");
    if (!source)
        SKIP("Set CROWNY_TEST_OSL_TEXTURE_SOURCE to the image_texture OSL fixture.");
    OslShaderImporter importer;
    const auto asset = importer.Import(source, importer.CreateImportOptions());
    REQUIRE(asset);
    const auto shader = StaticRefCast<Shader>(asset);
    REQUIRE(shader->GetTechniques().size() == 1);
    const auto& pass = shader->GetTechniques().front()->GetRenderPasses().front();
    CHECK_FALSE(pass->GetGraphicsPipeline());
    const auto& description = pass->GetPassDesc().FragmentShader->Description;
    for (const auto& [name, slot] : { std::pair{ "osl_image", 15u }, std::pair{ "osl_detail", 16u } })
    {
        REQUIRE(description->Textures.contains(name));
        const auto& texture = description->Textures.at(name);
        CHECK(texture.Type == SAMPLER2D);
        CHECK(texture.Set == 0);
        CHECK(texture.Slot == slot);
        CHECK(texture.ArraySize == 1);
        CHECK_FALSE(texture.RuntimeArray);
        REQUIRE(description->Annotations.contains(name));
        CHECK(description->Annotations.at(name).HasDefault);
        CHECK(description->Annotations.at(name).DefaultValueStr == "black");
    }
    CHECK(description->Annotations.at("osl_image").DisplayName == "Image");
    CHECK(description->Annotations.at("osl_detail").DisplayName == "Detail image");
    CHECK(description->Uniforms.at("OslInputs").Members.size() == 2);
}

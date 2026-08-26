#include "Crowny/Assets/AssetCodecs.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Import/ImportOptions.h"
#include "Crowny/Renderer/ShaderVariation.h"
#include "Crowny/Serialization/ImportOptionsSerializer.h"
#include "Crowny/Utils/BuiltInShaderCompiler.h"
#include "Crowny/Utils/ShaderCompiler.h"
#include "Crowny/Utils/ShaderSourceParser.h"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <fstream>
#include <yaml-cpp/yaml.h>

using namespace Crowny;

namespace
{
    class ScopedAssetManagerModule
    {
    public:
        ScopedAssetManagerModule()
        {
            if (AssetManager::TryGet() == nullptr)
            {
                AssetManager::StartUp();
                m_OwnsModule = true;
            }
        }

        ~ScopedAssetManagerModule()
        {
            if (m_OwnsModule)
                AssetManager::Shutdown();
        }

    private:
        bool m_OwnsModule = false;
    };

    bool HasErrorContaining(const ParsedShaderSource& source, StringView text)
    {
        return std::any_of(source.Diagnostics.begin(), source.Diagnostics.end(), [&](const ShaderDiagnostic& diagnostic) {
            return diagnostic.Severity == ShaderDiagnosticSeverity::Error && diagnostic.Message.find(text) != String::npos;
        });
    }

    size_t CountErrorsContaining(const ParsedShaderSource& source, StringView text)
    {
        return static_cast<size_t>(std::count_if(source.Diagnostics.begin(), source.Diagnostics.end(), [&](const ShaderDiagnostic& diagnostic) {
            return diagnostic.Severity == ShaderDiagnosticSeverity::Error && diagnostic.Message.find(text) != String::npos;
        }));
    }

    const String BASIC_VARIATION_SHADER = R"(#lang glsl
#pragma variation USE_FOG
#type vertex
#version 450
layout(location = 0) in vec3 cw_Position;
void main() { gl_Position = vec4(cw_Position, 1.0); }
#type fragment
#version 450
layout(location = 0) out vec4 outColor;
void main() {
#ifdef USE_FOG
    outColor = vec4(0.25);
#else
    outColor = vec4(1.0);
#endif
}
)";
} // namespace

TEST_CASE("Shader source parser separates passes, stages, and variations", "[Shader]")
{
    const String source = R"(#lang glsl
#pragma variation USE_FOG
#pragma variation_multi _ QUALITY_LOW QUALITY_HIGH
#pragma cull back
#pass graphics
#type vertex
#version 450
// VERTEX_ONLY
void main() {}
#type fragment
#version 450
// FRAGMENT_ONLY
void main() {}
#pass lighting
#type compute
#version 450
// COMPUTE_ONLY
void main() {}
)";

    const ParsedShaderSource parsed = ShaderSourceParser::Parse("pipeline.glsl", source);
    REQUIRE(parsed.Succeeded());
    REQUIRE(parsed.Passes.size() == 2);
    CHECK(parsed.Passes[0].Name == "graphics");
    CHECK(parsed.Passes[0].HasStage[VERTEX_SHADER]);
    CHECK(parsed.Passes[0].HasStage[FRAGMENT_SHADER]);
    CHECK_FALSE(parsed.Passes[0].HasStage[COMPUTE_SHADER]);
    CHECK(parsed.Passes[1].HasStage[COMPUTE_SHADER]);
    CHECK(parsed.Passes[0].Stages[VERTEX_SHADER].find("VERTEX_ONLY") != String::npos);
    CHECK(parsed.Passes[0].Stages[VERTEX_SHADER].find("FRAGMENT_ONLY") == String::npos);
    CHECK(parsed.Passes[1].Stages[COMPUTE_SHADER].find("COMPUTE_ONLY") != String::npos);
    CHECK(parsed.Passes[0].Stages[VERTEX_SHADER].find("#type") == String::npos);
    REQUIRE(parsed.VariationGroups.size() == 2);
    CHECK(parsed.VariationCount == 6);
}

TEST_CASE("Shader source parser rejects malformed topology and directives", "[Shader]")
{
    SECTION("Duplicate pass and stage")
    {
        const ParsedShaderSource parsed = ShaderSourceParser::Parse("duplicate.glsl", R"(#pass repeated
#type vertex
void main() {}
#type vertex
void main() {}
#pass repeated
#type compute
void main() {}
)");
        CHECK_FALSE(parsed.Succeeded());
        CHECK(CountErrorsContaining(parsed, "more than once") >= 2);
    }

    SECTION("Mixed pipeline kinds")
    {
        const ParsedShaderSource parsed = ShaderSourceParser::Parse("mixed.glsl", R"(#type vertex
void main() {}
#type fragment
void main() {}
#type compute
void main() {}
)");
        CHECK_FALSE(parsed.Succeeded());
        CHECK(HasErrorContaining(parsed, "cannot mix"));
    }

    SECTION("Invalid identifier")
    {
        const ParsedShaderSource parsed = ShaderSourceParser::Parse("identifier.glsl", R"(#pragma variation 4_BAD
#type compute
void main() {}
)");
        CHECK_FALSE(parsed.Succeeded());
        CHECK(HasErrorContaining(parsed, "valid preprocessor identifier"));
    }
}

TEST_CASE("Shader compiler rejects malformed blend-state values", "[Shader]")
{
    String source = BASIC_VARIATION_SHADER;
    const size_t insertion = source.find("layout(location = 0) in");
    REQUIRE(insertion != String::npos);
    source.insert(insertion, "blend_state { enabled = true; color = { invalid, zero, add }; };\n");

    const ShaderCompileResult result = ShaderCompiler::CompileWithDiagnostics("blend.glsl", source);
    CHECK_FALSE(result.Succeeded());
    CHECK(std::any_of(result.Diagnostics.begin(), result.Diagnostics.end(), [](const ShaderDiagnostic& diagnostic) {
        return diagnostic.Message.find("Invalid color blend equation") != String::npos;
    }));
}

TEST_CASE("Shader compiler applies reverse-Z depth compare pragmas", "[Shader]")
{
    const String source = R"(#lang glsl
#pragma depth_read true
#pragma depth_write true
#pragma depth_compare greater_equal
#type vertex
#version 450
layout(location = 0) in vec3 cw_Position;
void main() { gl_Position = vec4(cw_Position, 1.0); }
#type fragment
#version 450
void main() {}
)";

    const ShaderCompileResult result = ShaderCompiler::CompileWithDiagnostics("reverse_z_depth.glsl", source);
    REQUIRE(result.Succeeded());
    REQUIRE(result.Description.Techniques.size() == 1);
    const ShaderRenderPassDesc& pass = result.Description.Techniques[0]->GetRenderPasses()[0]->GetPassDesc();
    REQUIRE(pass.DepthStencilState);
    CHECK(pass.DepthStencilState->EnableDepthRead);
    CHECK(pass.DepthStencilState->EnableDepthWrite);
    CHECK(pass.DepthStencilState->DepthCompareFunction == CompareFunction::GREATER_EQUAL);
}

TEST_CASE("Shader includes expand relative files and reject cycles", "[Shader]")
{
    const Path directory = std::filesystem::temp_directory_path() / "crowny_shader_include_tests";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    const Path common = directory / "common.glslinc";
    const Path nested = directory / "nested.glslinc";
    const Path shader = directory / "shader.glsl";
    {
        std::ofstream stream(common, std::ios::binary);
        stream << "vec3 SharedLighting() { return vec3(1.0); }\n";
    }
    {
        std::ofstream stream(nested, std::ios::binary);
        stream << "#include \"common.glslinc\"\n";
    }

    const ShaderPreprocessResult initial =
      ShaderCompiler::PreprocessIncludes(shader, "#include \"nested.glslinc\"\n#include \"common.glslinc\"\nvoid main() {}\n");
    REQUIRE(initial.Succeeded());
    CHECK(initial.Diagnostics.empty());
    CHECK(initial.Source.find("SharedLighting") != String::npos);
    REQUIRE(initial.Dependencies.size() == 2);
    CHECK(initial.Dependencies[0] == std::filesystem::weakly_canonical(nested));
    CHECK(initial.Dependencies[1] == std::filesystem::weakly_canonical(common));
    CHECK(initial.ContentHash != 0);

    const auto originalWriteTime = std::filesystem::last_write_time(common, error);
    REQUIRE_FALSE(error);
    std::filesystem::last_write_time(common, originalWriteTime + std::chrono::hours(1), error);
    REQUIRE_FALSE(error);
    const ShaderPreprocessResult touched =
      ShaderCompiler::PreprocessIncludes(shader, "#include \"nested.glslinc\"\n#include \"common.glslinc\"\nvoid main() {}\n");
    REQUIRE(touched.Succeeded());
    CHECK(touched.ContentHash == initial.ContentHash);

    {
        std::ofstream stream(common, std::ios::binary | std::ios::trunc);
        stream << "vec3 SharedLighting() { return vec3(0.5); }\n";
    }
    const ShaderPreprocessResult changed =
      ShaderCompiler::PreprocessIncludes(shader, "#include \"nested.glslinc\"\n#include \"common.glslinc\"\nvoid main() {}\n");
    REQUIRE(changed.Succeeded());
    CHECK(changed.ContentHash != initial.ContentHash);

    {
        std::ofstream stream(common, std::ios::binary | std::ios::trunc);
        stream << "#include \"nested.glslinc\"\n";
    }
    const ShaderPreprocessResult cyclic = ShaderCompiler::PreprocessIncludes(shader, "#include \"nested.glslinc\"\n");
    CHECK_FALSE(cyclic.Succeeded());
    CHECK(cyclic.ContentHash == 0);
    CHECK(std::any_of(cyclic.Diagnostics.begin(), cyclic.Diagnostics.end(), [](const ShaderDiagnostic& diagnostic) {
        return diagnostic.Message.find("cycle") != String::npos;
    }));

    std::filesystem::remove_all(directory, error);
}

TEST_CASE("Shader compiler cache invalidates stages that consume a changed include", "[Shader]")
{
    const Path directory = std::filesystem::temp_directory_path() / "crowny_shader_include_cache_tests";
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    std::filesystem::create_directories(directory, error);
    const Path include = directory / "color.glslinc";
    const Path shader = directory / "shader.glsl";
    {
        std::ofstream stream(include, std::ios::binary);
        stream << "vec4 SharedColor() { return vec4(1.0); }\n";
    }
    const String source = R"(#lang glsl
#type vertex
#version 450
void main() { gl_Position = vec4(0.0); }
#type fragment
#version 450
#include "color.glslinc"
layout(location = 0) out vec4 outColor;
void main() { outColor = SharedColor(); }
)";

    ShaderCompiler::ClearCache();
    const ShaderCompileResult first = ShaderCompiler::CompileWithDiagnostics(shader, source);
    REQUIRE(first.Succeeded());
    const ShaderCompilerCacheStats afterFirst = ShaderCompiler::GetCacheStats();
    CHECK(afterFirst.Misses == 2);

    const ShaderCompileResult unchanged = ShaderCompiler::CompileWithDiagnostics(shader, source);
    REQUIRE(unchanged.Succeeded());
    CHECK(unchanged.SourceContentHash == first.SourceContentHash);
    const ShaderCompilerCacheStats afterUnchanged = ShaderCompiler::GetCacheStats();
    CHECK(afterUnchanged.Hits == afterFirst.Hits + 2);

    {
        std::ofstream stream(include, std::ios::binary | std::ios::trunc);
        stream << "vec4 SharedColor() { return vec4(0.5); }\n";
    }
    const ShaderCompileResult changed = ShaderCompiler::CompileWithDiagnostics(shader, source);
    REQUIRE(changed.Succeeded());
    CHECK(changed.SourceContentHash != first.SourceContentHash);
    const ShaderCompilerCacheStats afterChanged = ShaderCompiler::GetCacheStats();
    CHECK(afterChanged.Misses == afterUnchanged.Misses + 1);
    CHECK(afterChanged.Hits == afterUnchanged.Hits + 1);

    std::filesystem::remove_all(directory, error);
}

TEST_CASE("Built-in shader freshness follows transitive include content", "[Shader][Assets]")
{
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const Path directory = std::filesystem::temp_directory_path() / ("crowny_builtin_shader_" + std::to_string(unique));
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    REQUIRE_FALSE(error);
    const Path include = directory / "color.glslinc";
    const Path nestedInclude = directory / "lighting.glslinc";
    const Path shader = directory / "freshness.glsl";
    Path asset = shader;
    asset.replace_extension(".asset");

    {
        std::ofstream stream(include, std::ios::binary);
        stream << "vec4 SharedColor() { return vec4(1.0); }\n";
    }
    {
        std::ofstream stream(nestedInclude, std::ios::binary);
        stream << "#include \"color.glslinc\"\n";
    }
    {
        std::ofstream stream(shader, std::ios::binary);
        stream << R"(#lang glsl
#type vertex
#version 450
void main() { gl_Position = vec4(0.0); }
#type fragment
#version 450
#include "lighting.glslinc"
layout(location = 0) out vec4 outColor;
void main() { outColor = SharedColor(); }
)";
    }

    ScopedAssetManagerModule assetManager;
    const BuiltInShaderCompileStats first = BuiltInShaderCompiler::CompileAll(directory);
    CHECK(first.Compiled == 1);
    CHECK(first.Skipped == 0);
    CHECK(first.Failed == 0);
    AssetFileHeader firstHeader;
    REQUIRE(PeekAssetHeader(asset, firstHeader));
    const uint64_t firstHash = firstHeader.SourceContentHash;
    std::ifstream shaderStream(shader, std::ios::binary);
    const String shaderSource((std::istreambuf_iterator<char>(shaderStream)), std::istreambuf_iterator<char>());
    CHECK(firstHash == ShaderCompiler::PreprocessIncludes(shader, shaderSource).ContentHash);

    const auto originalWriteTime = std::filesystem::last_write_time(include, error);
    REQUIRE_FALSE(error);
    std::filesystem::last_write_time(include, originalWriteTime + std::chrono::hours(1), error);
    REQUIRE_FALSE(error);
    const BuiltInShaderCompileStats touched = BuiltInShaderCompiler::CompileAll(directory);
    CHECK(touched.Compiled == 0);
    CHECK(touched.Skipped == 1);
    CHECK(touched.Failed == 0);

    {
        std::ofstream stream(include, std::ios::binary | std::ios::trunc);
        stream << "vec4 SharedColor() { return vec4(0.25); }\n";
    }
    const BuiltInShaderCompileStats changed = BuiltInShaderCompiler::CompileAll(directory);
    CHECK(changed.Compiled == 1);
    CHECK(changed.Skipped == 0);
    CHECK(changed.Failed == 0);
    AssetFileHeader changedHeader;
    REQUIRE(PeekAssetHeader(asset, changedHeader));
    CHECK(changedHeader.SourceContentHash != firstHash);

    std::filesystem::remove(include, error);
    const BuiltInShaderCompileStats missing = BuiltInShaderCompiler::CompileAll(directory);
    CHECK(missing.Compiled == 0);
    CHECK(missing.Skipped == 0);
    CHECK(missing.Failed == 1);
    AssetFileHeader preservedHeader;
    REQUIRE(PeekAssetHeader(asset, preservedHeader));
    CHECK(preservedHeader.SourceContentHash == changedHeader.SourceContentHash);

    std::filesystem::remove_all(directory, error);
}

TEST_CASE("Shader variation expansion has a fixed upper bound", "[Shader]")
{
    String source = "#lang glsl\n";
    for (uint32_t index = 0; index < 9; ++index)
        source += "#pragma variation OPTION_" + std::to_string(index) + "\n";
    source += "#type compute\n#version 450\nvoid main() {}\n";

    const ParsedShaderSource parsed = ShaderSourceParser::Parse("too_many.glsl", source);
    CHECK_FALSE(parsed.Succeeded());
    CHECK(HasErrorContaining(parsed, "limit is 256"));
}

TEST_CASE("Shader define and variation keys are deterministic and type-sensitive", "[Shader]")
{
    ShaderDefines firstDefines;
    firstDefines.Set("BETA", "2");
    firstDefines.Set("ALPHA", "1");
    ShaderDefines secondDefines;
    secondDefines.Set("ALPHA", "1");
    secondDefines.Set("BETA", "2");
    CHECK(firstDefines.GetCanonicalKey() == secondDefines.GetCanonicalKey());
    CHECK(firstDefines.GetHash() == secondDefines.GetHash());

    ShaderDefines preciseFloat;
    preciseFloat.Set("VALUE", 0.123456789f);
    CHECK(preciseFloat.Get().at("VALUE") != "0.123457");

    ShaderVariation firstVariation;
    firstVariation.Set("ROUGHNESS", 0.5f);
    firstVariation.Set("SKINNED", true);
    ShaderVariation secondVariation;
    secondVariation.Set("SKINNED", true);
    secondVariation.Set("ROUGHNESS", 0.5f);
    CHECK(firstVariation.GetCanonicalKey() == secondVariation.GetCanonicalKey());
    CHECK(firstVariation.GetHash() == secondVariation.GetHash());

    ShaderVariation integerVariation;
    integerVariation.Set("SKINNED", 1);
    CHECK_FALSE(firstVariation.Matches(integerVariation, false));
}

TEST_CASE("Shader compiler reuses identical stages and invalidates changed source", "[Shader]")
{
    ShaderCompiler::ClearCache();
    const ShaderCompileResult first = ShaderCompiler::CompileWithDiagnostics("cache.glsl", BASIC_VARIATION_SHADER);
    REQUIRE(first.Succeeded());
    REQUIRE(first.Description.Techniques.size() == 2);
    const Ref<BinaryShaderData>& vertex = first.Description.Techniques[0]->GetRenderPasses()[0]->GetPassDesc().VertexShader;
    REQUIRE(vertex);
    CHECK(vertex->EntryPoint == "main");
    const ShaderCompilerCacheStats afterFirst = ShaderCompiler::GetCacheStats();
    CHECK(afterFirst.Misses == 4);
    CHECK(afterFirst.Hits == 0);

    const ShaderCompileResult second = ShaderCompiler::CompileWithDiagnostics("cache.glsl", BASIC_VARIATION_SHADER);
    REQUIRE(second.Succeeded());
    const ShaderCompilerCacheStats afterSecond = ShaderCompiler::GetCacheStats();
    CHECK(afterSecond.Misses == afterFirst.Misses);
    CHECK(afterSecond.Hits == afterFirst.Hits + 4);

    String changedSource = BASIC_VARIATION_SHADER;
    changedSource += "\n// cache invalidation\n";
    const ShaderCompileResult changed = ShaderCompiler::CompileWithDiagnostics("cache.glsl", changedSource);
    REQUIRE(changed.Succeeded());
    const ShaderCompilerCacheStats afterChanged = ShaderCompiler::GetCacheStats();
    CHECK(afterChanged.Misses == afterSecond.Misses + 4);
}

TEST_CASE("Shader reflection keeps storage buffers and images", "[Shader]")
{
    const String source = R"(#lang glsl
#type compute
#version 450
layout(local_size_x = 1) in;
layout(std430, set = 1, binding = 2) readonly buffer InputData { float values[]; } inputData;
layout(set = 1, binding = 3, rgba8) uniform writeonly image2D outputImage;
void main() { imageStore(outputImage, ivec2(0), vec4(inputData.values[0])); }
)";
    const ShaderCompileResult result = ShaderCompiler::CompileWithDiagnostics("reflection.glsl", source);
    REQUIRE(result.Succeeded());
    const Ref<BinaryShaderData>& compute = result.Description.Techniques[0]->GetRenderPasses()[0]->GetPassDesc().ComputeShader;
    REQUIRE(compute);
    REQUIRE(compute->Description);
    REQUIRE(compute->Description->Buffers.contains("inputData"));
    CHECK(compute->Description->Buffers.at("inputData").Set == 1);
    CHECK(compute->Description->Buffers.at("inputData").Slot == 2);
    REQUIRE(compute->Description->LoadStoreTextures.contains("outputImage"));
    CHECK(compute->Description->LoadStoreTextures.at("outputImage").Type == RWTEXTURE2D);
}

TEST_CASE("Shader reflection preserves runtime descriptor arrays", "[Shader]")
{
    const String source = R"(#lang glsl
#type vertex
#version 450
void main()
{
    const vec2 positions[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
#type fragment
#version 450
#extension GL_EXT_nonuniform_qualifier : require
layout(set = 1, binding = 1) uniform sampler2D textures[];
layout(location = 0) out vec4 color;
void main() { color = texture(textures[nonuniformEXT(0)], vec2(0.5)); }
)";
    const ShaderCompileResult result = ShaderCompiler::CompileWithDiagnostics("descriptor_array.glsl", source);
    for (const ShaderDiagnostic& diagnostic : result.Diagnostics)
        INFO(diagnostic.Message);
    REQUIRE(result.Succeeded());
    const Ref<BinaryShaderData>& fragment =
      result.Description.Techniques[0]->GetRenderPasses()[0]->GetPassDesc().FragmentShader;
    REQUIRE(fragment);
    REQUIRE(fragment->Description);
    REQUIRE(fragment->Description->Textures.contains("textures"));
    const UniformResourceDesc& textures = fragment->Description->Textures.at("textures");
    CHECK(textures.Set == 1);
    CHECK(textures.Slot == 1);
    CHECK(textures.RuntimeArray);
}

TEST_CASE("GPU-driven renderer shaders compile together", "[Shader][Renderer]")
{
    const std::array<Path, 17> shaders = {
        "Crowny-Editor/Resources/Shaders/BinAndCompactIndirectDraws.glsl",
        "Crowny-Editor/Resources/Shaders/GpuDepthOnly.glsl",
        "Crowny-Editor/Resources/Shaders/GpuAnimatedDepthOnly.glsl",
        "Crowny-Editor/Resources/Shaders/GpuDepthObjectID.glsl",
        "Crowny-Editor/Resources/Shaders/GpuAnimatedDepthObjectID.glsl",
        "Crowny-Editor/Resources/Shaders/GpuShadowDepth.glsl",
        "Crowny-Editor/Resources/Shaders/BuildHiZ.glsl",
        "Crowny-Editor/Resources/Shaders/ForwardPlusStandard.glsl",
        "Crowny-Editor/Resources/Shaders/DeferredPlusStandard.glsl",
        "Crowny-Editor/Resources/Shaders/DeferredPlusLighting.glsl",
        "Crowny-Editor/Resources/Shaders/ToonOutlines.glsl",
        "Crowny-Editor/Resources/Shaders/Toon.glsl",
        "Crowny-Editor/Resources/Shaders/Gtao.glsl",
        "Crowny-Editor/Resources/Shaders/TemporalResolve.glsl",
        "Crowny-Editor/Resources/Shaders/Bloom.glsl",
        "Crowny-Editor/Resources/Shaders/Sky.glsl",
        "Crowny-Editor/Resources/Shaders/ToneMap.glsl",
    };

    for (const Path& path : shaders)
    {
        INFO("Compiling " << path.string());
        std::ifstream stream(path, std::ios::binary);
        REQUIRE(stream.good());
        const String source((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        const ShaderCompileResult result = ShaderCompiler::CompileWithDiagnostics(path, source);
        for (const ShaderDiagnostic& diagnostic : result.Diagnostics)
            INFO(diagnostic.Message);
        CHECK(result.Succeeded());
    }
}

TEST_CASE("Shader import options preserve language and sorted defines", "[Shader][Serialization]")
{
    Ref<ShaderImportOptions> options = CreateRef<ShaderImportOptions>();
    options->Language = ShaderLanguage::HLSL;
    options->SetDefine("ZETA", "9");
    options->SetDefine("ALPHA", "1");

    YAML::Emitter emitter;
    emitter << YAML::BeginMap;
    ImportOptionsSerializer::Serialize(emitter, options);
    emitter << YAML::EndMap;
    const String yaml = emitter.c_str();
    CHECK(yaml.find("ALPHA") < yaml.find("ZETA"));

    const Ref<ShaderImportOptions> restored = StaticRefCast<ShaderImportOptions>(ImportOptionsSerializer::Deserialize(YAML::Load(yaml)));
    REQUIRE(restored);
    CHECK(restored->Language == ShaderLanguage::HLSL);
    CHECK(restored->GetDefines() == options->GetDefines());
}

#include "Crowny/Utils/ShaderCompiler.h"

#include <catch2/catch_test_macros.hpp>
#include <cstring>

using namespace Crowny;

TEST_CASE("Offline compute SPIR-V preserves reflected storage buffer bindings", "[Shader][OSL]")
{
    const String source = R"(#version 450
layout(local_size_x=64) in;
layout(set=0,binding=0,std430) readonly buffer Samples { vec4 inputs[]; };
layout(set=0,binding=1,std430) writeonly buffer Colors { vec4 outputs[]; };
void main() { outputs[gl_GlobalInvocationID.x] = inputs[gl_GlobalInvocationID.x]; }
)";
    const auto compiled = ShaderCompiler::CompileStage(source, COMPUTE_SHADER, ShaderLanguage::GLSL, ShaderLanguage::VKSL, {});
    REQUIRE(compiled);
    REQUIRE_FALSE(compiled->Data.empty());
    String error;
    const auto loaded = ShaderCompiler::LoadComputeSpirv(compiled->Data, error);
    REQUIRE(loaded);
    CHECK(error.empty());
    CHECK(loaded->EntryPoint == "main");
    CHECK(loaded->Type == COMPUTE_SHADER);
    REQUIRE(loaded->Description);
    CHECK(loaded->Description->Buffers.size() == 2);
    uint32_t slots = 0;
    for (const auto& [name, buffer] : loaded->Description->Buffers)
    {
        CHECK(buffer.Set == 0);
        REQUIRE(buffer.Slot < 2);
        slots |= 1u << buffer.Slot;
    }
    CHECK(slots == 3);

    auto truncated = compiled->Data;
    truncated.pop_back();
    CHECK_FALSE(ShaderCompiler::LoadComputeSpirv(truncated, error));
    CHECK_FALSE(error.empty());
    auto badInstruction = compiled->Data;
    std::memset(badInstruction.data() + 20, 0, 4);
    CHECK_FALSE(ShaderCompiler::LoadComputeSpirv(badInstruction, error));
    CHECK_FALSE(error.empty());
}

TEST_CASE("Offline compute SPIR-V rejects graphics entry points and empty input", "[Shader][OSL]")
{
    String error;
    CHECK_FALSE(ShaderCompiler::LoadComputeSpirv({}, error));
    CHECK_FALSE(error.empty());
    const auto vertex = ShaderCompiler::CompileStage("#version 450\nvoid main() { gl_Position = vec4(0); }", VERTEX_SHADER, ShaderLanguage::GLSL,
                                                     ShaderLanguage::VKSL, {});
    REQUIRE(vertex);
    REQUIRE_FALSE(vertex->Data.empty());
    CHECK_FALSE(ShaderCompiler::LoadComputeSpirv(vertex->Data, error));
    CHECK(error.find("requested shader stage") != String::npos);
}

TEST_CASE("Offline graphics SPIR-V restores vertex attributes and fragment parameters", "[Shader][Procedural]")
{
    const auto vertex =
      ShaderCompiler::CompileStage("#version 450\nlayout(location=4) in vec3 cw_Position; void main() { gl_Position = vec4(cw_Position, 1); }",
                                   VERTEX_SHADER, ShaderLanguage::GLSL, ShaderLanguage::VKSL, {});
    const auto fragment = ShaderCompiler::CompileStage(
      "#version 450\nlayout(binding=13) uniform Params { vec4 tint; }; layout(location=0) out vec4 color; void main() { color = tint; }",
      FRAGMENT_SHADER, ShaderLanguage::GLSL, ShaderLanguage::VKSL, {});
    REQUIRE(vertex);
    REQUIRE(fragment);
    String error;
    const auto loadedVertex = ShaderCompiler::LoadSpirv(vertex->Data, VERTEX_SHADER, error);
    const auto loadedFragment = ShaderCompiler::LoadSpirv(fragment->Data, FRAGMENT_SHADER, error);
    REQUIRE(loadedVertex);
    REQUIRE(loadedFragment);
    CHECK(loadedVertex->VertexLayout.GetElements().size() == 1);
    REQUIRE(loadedFragment->Description->Uniforms.size() == 1);
    CHECK(loadedFragment->Description->Uniforms.begin()->second.Slot == 13);
    CHECK_FALSE(ShaderCompiler::LoadSpirv(fragment->Data, VERTEX_SHADER, error));
    CHECK_FALSE(ShaderCompiler::LoadSpirv(vertex->Data, FRAGMENT_SHADER, error));
}

#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/RenderAPI/UniformParamInfo.h"

#include <catch2/catch_test_macros.hpp>

using namespace Crowny;

namespace
{
    class TestUniformParamInfo final : public UniformParamInfo
    {
    public:
        explicit TestUniformParamInfo(const UniformParamDesc& desc) : UniformParamInfo(desc) {}
    };

    UniformResourceDesc Resource(String name, UniformResourceType type, uint32_t set, uint32_t slot)
    {
        UniformResourceDesc resource{};
        resource.Name = std::move(name);
        resource.Type = type;
        resource.Set = set;
        resource.Slot = slot;
        return resource;
    }
} // namespace

TEST_CASE("Uniform parameter binding queries reject sparse slots and wrong types", "[Renderer][Uniforms]")
{
    Ref<UniformDesc> compute = CreateRef<UniformDesc>();
    compute->Buffers.emplace("instances", Resource("instances", UniformResourceType::STRUCTURED_BUFFER, 1, 3));
    compute->LoadStoreTextures.emplace("output", Resource("output", UniformResourceType::RWTEXTURE2D, 2, 5));

    UniformParamDesc desc{};
    desc.ComputeParams = compute;
    TestUniformParamInfo info(desc);

    CHECK(info.HasBinding(UniformParamInfo::ParamType::Buffer, 1, 3));
    CHECK(info.HasBinding(UniformParamInfo::ParamType::LoadStoreTexture, 2, 5));
    CHECK_FALSE(info.HasBinding(UniformParamInfo::ParamType::Texture, 1, 3));
    CHECK_FALSE(info.HasBinding(UniformParamInfo::ParamType::Buffer, 1, 2));
    CHECK_FALSE(info.HasBinding(UniformParamInfo::ParamType::Buffer, 3, 0));
}

TEST_CASE("Uniform parameter binding queries preserve combined texture samplers", "[Renderer][Uniforms]")
{
    Ref<UniformDesc> fragment = CreateRef<UniformDesc>();
    fragment->Textures.emplace("albedoMap", Resource("albedoMap", UniformResourceType::TEXTURE2D, 0, 7));
    fragment->Samplers.emplace("albedoMap", Resource("albedoMap", UniformResourceType::SAMPLER2D, 0, 7));

    UniformParamDesc desc{};
    desc.FragmentParams = fragment;
    TestUniformParamInfo info(desc);

    CHECK(info.HasBinding(UniformParamInfo::ParamType::Texture, 0, 7));
    CHECK(info.HasBinding(UniformParamInfo::ParamType::SamplerState, 0, 7));
    CHECK_FALSE(info.HasBinding(UniformParamInfo::ParamType::Buffer, 0, 7));
}

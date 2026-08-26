#include <catch2/catch_test_macros.hpp>

#include "Crowny/Assets/AssetListener.h"
#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/Renderer/Material.h"
#include "Panels/MaterialInspectorSchemaCache.h"

#include <array>
#include <type_traits>
#include <utility>

using namespace Crowny;

static_assert(std::is_same_v<decltype(std::declval<const Material&>().GetTextures()), UniformDesc::TextureMap>);
static_assert(std::is_same_v<decltype(std::declval<const Material&>().GetTextureDescriptors()), const UniformDesc::TextureMap&>);

namespace
{
    class AssetListenerManagerScope
    {
    public:
        AssetListenerManagerScope() : m_Owned(!AssetListenerManager::IsStartedUp())
        {
            if (m_Owned)
                AssetListenerManager::StartUp();
        }

        ~AssetListenerManagerScope()
        {
            if (m_Owned)
                AssetListenerManager::Shutdown();
        }

    private:
        bool m_Owned;
    };

    class TestMaterialSchemaSource final : public MaterialInspectorSchemaSource
    {
    public:
        uint64_t GetLayoutVersion() const override { return LayoutVersion; }
        const Material::BindingMap& GetBindings() const override { return Bindings; }
        const UniformDesc::TextureMap& GetTextureDescriptors() const override { return Textures; }
        const UnorderedMap<String, AnnotationSet>& GetAnnotations(ShaderType shaderType) const override
        {
            return Annotations[static_cast<size_t>(shaderType)];
        }
        uint32_t GetBlockBindingSlot(const String& blockName) const override
        {
            const auto slot = BlockSlots.find(blockName);
            return slot != BlockSlots.end() ? slot->second : 0u;
        }

        uint64_t LayoutVersion = 1u;
        Material::BindingMap Bindings;
        UniformDesc::TextureMap Textures;
        std::array<UnorderedMap<String, AnnotationSet>, SHADER_COUNT> Annotations;
        UnorderedMap<String, uint32_t> BlockSlots;
    };

    Material::UniformMember Member(uint32_t offset, ShaderDataType type, String blockName)
    {
        Material::UniformMember member{};
        member.Offset = offset;
        member.DataType = type;
        member.BufferName = std::move(blockName);
        return member;
    }

    UniformResourceDesc TextureResource(String name, UniformResourceType type, uint32_t set, uint32_t slot)
    {
        UniformResourceDesc resource{};
        resource.Name = std::move(name);
        resource.Type = type;
        resource.Set = set;
        resource.Slot = slot;
        return resource;
    }

    TestMaterialSchemaSource MakeSchemaSource()
    {
        TestMaterialSchemaSource source;
        source.LayoutVersion = 100u;
        source.BlockSlots.emplace("Surface", 1u);
        source.BlockSlots.emplace("Details", 2u);
        source.Bindings.emplace("albedoColor", Member(0u, ShaderDataType::Float4, "Surface"));
        source.Bindings.emplace("roughness_value", Member(4u, ShaderDataType::Float, "Details"));
        source.Bindings.emplace("hiddenValue", Member(8u, ShaderDataType::Float, "Details"));
        source.Bindings.emplace("engineValue", Member(0u, ShaderDataType::Float, "cw_Frame"));
        source.Textures.emplace("albedoMap", TextureResource("albedoMap", TEXTURE2D, 1u, 3u));
        source.Textures.emplace("cw_ShadowMap", TextureResource("cw_ShadowMap", TEXTURE2D, 1u, 4u));

        source.Annotations[VERTEX_SHADER]["albedoColor"].DisplayName = "Vertex Color";
        AnnotationSet& color = source.Annotations[FRAGMENT_SHADER]["albedoColor"];
        color.DisplayName = "Base Color";
        color.IsColor = true;
        color.IsHDR = true;
        color.HasRange = true;
        color.RangeMin = 0.25f;
        color.RangeMax = 4.0f;
        source.Annotations[FRAGMENT_SHADER]["hiddenValue"].IsHidden = true;
        source.Annotations[FRAGMENT_SHADER]["albedoMap"].DisplayName = "Albedo Texture";
        return source;
    }
} // namespace

TEST_CASE("Material inspector schema preserves reflected editor metadata", "[Editor][Material][Schema]")
{
    TestMaterialSchemaSource source = MakeSchemaSource();
    MaterialInspectorSchemaCache cache;
    const Vector<ShaderParameterDesc>& parameters = cache.Resolve(source);

    REQUIRE(parameters.size() == 3u);
    CHECK(parameters[0].Identifier == "albedoColor");
    CHECK(parameters[0].DisplayName == "Base Color");
    CHECK(parameters[0].Type == ShaderParamType::Color4);
    CHECK(parameters[0].Flags.IsSet(ShaderParamFlag::HDR));
    CHECK(parameters[0].HasRange);
    CHECK(parameters[0].RangeMin == 0.25f);
    CHECK(parameters[0].RangeMax == 4.0f);
    CHECK(parameters[0].SortOrder == 1000u);

    CHECK(parameters[1].Identifier == "roughness_value");
    CHECK(parameters[1].DisplayName == "Roughness value");
    CHECK(parameters[1].Type == ShaderParamType::Float);
    CHECK(parameters[1].SortOrder == 2004u);

    CHECK(parameters[2].Identifier == "albedoMap");
    CHECK(parameters[2].DisplayName == "Albedo Texture");
    CHECK(parameters[2].Type == ShaderParamType::Texture2D);
    CHECK(parameters[2].Set == 1u);
    CHECK(parameters[2].Slot == 3u);
}

TEST_CASE("Material inspector schema cache invalidates only for a new layout", "[Editor][Material][Schema]")
{
    TestMaterialSchemaSource source = MakeSchemaSource();
    MaterialInspectorSchemaCache cache;
    CHECK(cache.Resolve(source)[0].DisplayName == "Base Color");

    source.Annotations[FRAGMENT_SHADER]["albedoColor"].DisplayName = "Changed Color";
    CHECK(cache.Resolve(source)[0].DisplayName == "Base Color");

    ++source.LayoutVersion;
    CHECK(cache.Resolve(source)[0].DisplayName == "Changed Color");
}

TEST_CASE("Material inspector schema cache retains no material resources", "[Editor][Material][Memory]")
{
    AssetListenerManagerScope listenerManager;
    Ref<Material> first = CreateRef<Material>(AssetHandle<Shader>());
    Ref<Material> second = CreateRef<Material>(AssetHandle<Shader>());
    MaterialInspectorSchemaCache cache;

    REQUIRE(first->GetRefCount() == 1u);
    CHECK(first->GetLayoutVersion() != second->GetLayoutVersion());
    CHECK(cache.Resolve(*first).empty());
    CHECK(first->GetRefCount() == 1u);

    const uint64_t layoutVersion = first->GetLayoutVersion();
    first->SetShader(nullptr);
    CHECK(first->GetLayoutVersion() != layoutVersion);
    CHECK(first->GetPassCount() == 0u);
    CHECK(first->GetBindings().empty());

    cache.Reset();
    CHECK(first->GetRefCount() == 1u);
}

TEST_CASE("Material inspector schema resolves without allocations after warm-up", "[Editor][Material][Memory][Frame]")
{
    TestMaterialSchemaSource source = MakeSchemaSource();
    MaterialInspectorSchemaCache cache;
    REQUIRE(cache.Resolve(source).size() == 3u);

    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
    for (uint32_t frame = 0; frame < 120u; frame++)
        cache.Resolve(source);
    const Memory::ThreadAllocationSnapshot after = Memory::GetThreadAllocationSnapshot();
    const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, after);

    CHECK(delta.AllocationCount == 0u);
    CHECK(delta.RequestedBytes == 0u);
}

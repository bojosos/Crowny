#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/GpuMaterial.h"

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

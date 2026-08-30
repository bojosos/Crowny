#pragma once

#include "Crowny/Common/Types.h"
#include "Crowny/Renderer/RenderTypes.h"

#include <cstddef>

namespace Crowny
{
    class Material;

    enum class MaterialModel : uint8_t
    {
        Standard,
        Unlit,
        Toon
    };

    enum class MaterialRenderRoute : uint8_t
    {
        StandardGpu,
        ForwardOnly,
        Unsupported
    };

    struct MaterialRenderClassification
    {
        MaterialModel Model = MaterialModel::Standard;
        AlphaMode Alpha = AlphaMode::Opaque;
        MaterialRenderRoute Route = MaterialRenderRoute::StandardGpu;

        bool UsesStandardGpuRecord() const { return Route == MaterialRenderRoute::StandardGpu; }
        bool IsForwardOnlyOpaque() const { return Route == MaterialRenderRoute::ForwardOnly && Alpha == AlphaMode::Opaque; }
        bool IsUnsupported() const { return Route == MaterialRenderRoute::Unsupported; }
        uint32_t GetMaterialTemplate() const { return UsesStandardGpuRecord() ? static_cast<uint32_t>(Model) : 0u; }
    };

    // Converts explicit shader metadata into one deterministic render route.
    // Unmarked third-party shaders fail closed as unsupported rather than being
    // interpreted as the engine's standard layout or a reverse-Z compatible pass.
    class MaterialRenderClassifier
    {
    public:
        static MaterialRenderClassification Classify(StringView shaderName, const Vector<String>& techniqueTags, bool hasBlending, bool alphaMasked);
        static MaterialRenderClassification Classify(const Material& material);
    };

    enum class ToonPatternMapping : uint8_t
    {
        UV,
        Triplanar,
        Screen
    };

    enum class GpuMaterialFlags : uint32_t
    {
        None = 0,
        TwoSided = 1 << 0,
        UseVertexColor = 1 << 1,
        ReceiveShadows = 1 << 2
    };

    constexpr GpuMaterialFlags operator|(GpuMaterialFlags first, GpuMaterialFlags second)
    {
        return static_cast<GpuMaterialFlags>(static_cast<uint32_t>(first) | static_cast<uint32_t>(second));
    }

    struct StandardMaterialDesc
    {
        static constexpr uint32_t MissingTexture = 0;

        glm::vec4 BaseColor = glm::vec4(1.0f);
        glm::vec3 Emissive = glm::vec3(0.0f);
        float EmissiveIntensity = 1.0f;
        float AlphaCutoff = 0.5f;
        float Metallic = 0.0f;
        float Roughness = 0.5f;
        float NormalScale = 1.0f;
        float AmbientOcclusion = 1.0f;
        uint32_t BaseColorTexture = MissingTexture;
        uint32_t NormalTexture = MissingTexture;
        uint32_t MetallicRoughnessTexture = MissingTexture;
        uint32_t AmbientOcclusionTexture = MissingTexture;
        uint32_t EmissiveTexture = MissingTexture;
        uint32_t ToonPatternTexture = MissingTexture;
        uint32_t ToonRampTexture = MissingTexture;
        uint32_t ToonMatcapTexture = MissingTexture;
        uint32_t SamplerIndex = 0;
        MaterialModel Model = MaterialModel::Standard;
        AlphaMode Alpha = AlphaMode::Opaque;
        GpuMaterialFlags Flags = GpuMaterialFlags::ReceiveShadows;

        glm::vec3 ToonShadowColor = glm::vec3(0.2f, 0.22f, 0.3f);
        float ToonBands = 3.0f;
        glm::vec3 ToonSpecularColor = glm::vec3(1.0f);
        float ToonSpecularThreshold = 0.8f;
        glm::vec3 ToonRimColor = glm::vec3(1.0f);
        float ToonRimThreshold = 0.5f;
        float ToonBandSmoothness = 0.08f;
        float ToonSpecularSmoothness = 0.05f;
        float ToonSpecularStrength = 0.5f;
        float ToonRimSmoothness = 0.08f;
        float ToonRimPower = 3.0f;
        float ToonRimStrength = 0.5f;
        float ToonRimShadowMask = 0.75f;
        float ToonIndirectStrength = 0.5f;
        float ToonPatternScale = 16.0f;
        float ToonPatternStrength = 0.0f;
        float ToonPatternSmoothness = 0.1f;
        float ToonPatternDistanceFade = 50.0f;
        ToonPatternMapping ToonPatternMappingMode = ToonPatternMapping::UV;
        float ToonRampStrength = 0.0f;
        float ToonRampOffset = 0.0f;
        float ToonMatcapStrength = 0.0f;
        float ToonMatcapRotation = 0.0f;
        glm::vec4 ToonOutlineColor = glm::vec4(0.02f, 0.02f, 0.025f, 1.0f);
        float ToonOutlineWidth = 1.5f;
        float ToonOutlineDepthThreshold = 0.002f;
        float ToonOutlineNormalThreshold = 0.2f;
        float ToonOutlineDistanceFade = 100.0f;
        float ToonSilhouetteWidth = 0.0f;
    };

    // Shared std430 record used by depth, shadow, Forward+, and G-buffer
    // variants. Texture zero is the engine missing/default texture.
    struct alignas(16) GpuMaterialData
    {
        glm::vec4 BaseColor = glm::vec4(1.0f);
        glm::vec4 EmissiveAlphaCutoff = glm::vec4(0.0f, 0.0f, 0.0f, 0.5f);
        glm::vec4 MetallicRoughnessNormalAo = glm::vec4(0.0f, 0.5f, 1.0f, 1.0f);
        glm::uvec4 TextureIndices0 = glm::uvec4(0u);
        glm::uvec4 TextureIndices1 = glm::uvec4(0u);
        glm::vec4 ToonShadowBands = glm::vec4(0.2f, 0.22f, 0.3f, 3.0f);
        glm::vec4 ToonSpecular = glm::vec4(1.0f, 1.0f, 1.0f, 0.8f);
        glm::vec4 ToonRim = glm::vec4(1.0f, 1.0f, 1.0f, 0.5f);
        glm::vec4 ToonControls = glm::vec4(0.08f, 0.05f, 0.5f, 0.08f);
        glm::vec4 ToonArtistic = glm::vec4(3.0f, 0.5f, 0.75f, 0.5f);
        glm::vec4 ToonPattern = glm::vec4(16.0f, 0.0f, 0.1f, 50.0f);
        glm::vec4 ToonOutlineColor = glm::vec4(0.02f, 0.02f, 0.025f, 1.0f);
        glm::vec4 ToonOutline = glm::vec4(1.5f, 0.002f, 0.2f, 100.0f);
        glm::vec4 ToonSilhouette = glm::vec4(0.0f);
        glm::vec4 ToonStyle = glm::vec4(0.0f);
        glm::uvec4 TextureIndices2 = glm::uvec4(0u);
    };

    static_assert(sizeof(GpuMaterialData) == 256, "GPU material records must preserve std430 float4 alignment");
    static_assert(offsetof(GpuMaterialData, ToonOutline) == 192);
    static_assert(offsetof(GpuMaterialData, ToonSilhouette) == 208);
    static_assert(offsetof(GpuMaterialData, ToonStyle) == 224);
    static_assert(offsetof(GpuMaterialData, TextureIndices2) == 240);

    class GpuMaterialPacker
    {
    public:
        static constexpr uint32_t UnsupportedModelAndAlpha = 0xffffffffu;

        static GpuMaterialData Pack(const StandardMaterialDesc& desc);
        static GpuMaterialData PackUnsupported();
        static uint32_t PackModelAndAlpha(MaterialModel model, AlphaMode alpha)
        {
            return static_cast<uint32_t>(model) | (static_cast<uint32_t>(alpha) << 8u);
        }
    };
} // namespace Crowny

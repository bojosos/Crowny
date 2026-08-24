#include "cwpch.h"

#include "Crowny/Renderer/GpuMaterial.h"

namespace Crowny
{
    GpuMaterialData GpuMaterialPacker::Pack(const StandardMaterialDesc& desc)
    {
        GpuMaterialData output;
        output.BaseColor = glm::max(desc.BaseColor, glm::vec4(0.0f));
        output.EmissiveAlphaCutoff = {
            glm::max(desc.Emissive, glm::vec3(0.0f)) * std::max(desc.EmissiveIntensity, 0.0f),
            std::clamp(desc.AlphaCutoff, 0.0f, 1.0f)
        };
        output.MetallicRoughnessNormalAo = {
            std::clamp(desc.Metallic, 0.0f, 1.0f), std::clamp(desc.Roughness, 0.045f, 1.0f),
            std::max(desc.NormalScale, 0.0f), std::clamp(desc.AmbientOcclusion, 0.0f, 1.0f)
        };
        output.TextureIndices0 = {
            desc.BaseColorTexture, desc.NormalTexture, desc.MetallicRoughnessTexture, desc.AmbientOcclusionTexture
        };
        output.TextureIndices1 = {
            desc.EmissiveTexture, desc.SamplerIndex, static_cast<uint32_t>(desc.Flags),
            PackModelAndAlpha(desc.Model, desc.Alpha)
        };
        output.ToonShadowBands = { glm::max(desc.ToonShadowColor, glm::vec3(0.0f)),
                                   std::clamp(desc.ToonBands, 2.0f, 16.0f) };
        output.ToonSpecular = { glm::max(desc.ToonSpecularColor, glm::vec3(0.0f)),
                                std::clamp(desc.ToonSpecularThreshold, 0.0f, 1.0f) };
        output.ToonRim = { glm::max(desc.ToonRimColor, glm::vec3(0.0f)),
                           std::clamp(desc.ToonRimThreshold, 0.0f, 1.0f) };
        output.ToonControls = {
            std::clamp(desc.ToonBandSmoothness, 0.0f, 0.5f),
            std::clamp(desc.ToonSpecularSmoothness, 0.0f, 0.5f),
            std::max(desc.ToonSpecularStrength, 0.0f),
            std::clamp(desc.ToonRimSmoothness, 0.0f, 0.5f)
        };
        output.ToonArtistic = {
            std::max(desc.ToonRimPower, 0.01f), std::max(desc.ToonRimStrength, 0.0f),
            std::clamp(desc.ToonRimShadowMask, 0.0f, 1.0f), std::max(desc.ToonIndirectStrength, 0.0f)
        };
        output.ToonPattern = {
            std::max(desc.ToonPatternScale, 0.001f), std::clamp(desc.ToonPatternStrength, 0.0f, 1.0f),
            std::clamp(desc.ToonPatternSmoothness, 0.0f, 0.5f), std::max(desc.ToonPatternDistanceFade, 0.0f)
        };
        output.ToonOutlineColor = glm::max(desc.ToonOutlineColor, glm::vec4(0.0f));
        output.ToonOutline = {
            std::max(desc.ToonOutlineWidth, 0.0f), std::max(desc.ToonOutlineDepthThreshold, 0.0f),
            std::clamp(desc.ToonOutlineNormalThreshold, 0.0f, 1.0f), std::max(desc.ToonOutlineDistanceFade, 0.0f)
        };
        output.ToonStyle = {
            std::clamp(desc.ToonRampStrength, 0.0f, 1.0f), std::clamp(desc.ToonRampOffset, -1.0f, 1.0f),
            std::clamp(desc.ToonMatcapStrength, 0.0f, 1.0f), desc.ToonMatcapRotation
        };
        output.TextureIndices2 = {
            desc.ToonPatternTexture, desc.ToonRampTexture, desc.ToonMatcapTexture,
            static_cast<uint32_t>(desc.ToonPatternMappingMode)
        };
        return output;
    }
} // namespace Crowny

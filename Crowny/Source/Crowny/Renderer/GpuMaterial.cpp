#include "cwpch.h"

#include "Crowny/Renderer/GpuMaterial.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Renderer/Material.h"

#include <cctype>

namespace Crowny
{
    namespace
    {
        String Lower(StringView value)
        {
            String result(value);
            std::transform(result.begin(), result.end(), result.begin(),
                           [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return result;
        }

        String ShaderStem(StringView shaderName)
        {
            const Path path{ String(shaderName) };
            return Lower(path.stem().string());
        }
    } // namespace

    MaterialRenderClassification MaterialRenderClassifier::Classify(StringView shaderName, const Vector<String>& techniqueTags, bool hasBlending,
                                                                    bool alphaMasked)
    {
        MaterialRenderClassification result;
        result.Alpha = hasBlending ? AlphaMode::Premultiplied : AlphaMode::Opaque;
        if (alphaMasked)
            result.Alpha = AlphaMode::Mask;

        String model;
        for (const String& tag : techniqueTags)
        {
            static constexpr StringView prefix = "material_model=";
            if (tag.size() <= prefix.size() || !StringView(tag).starts_with(prefix))
                continue;
            model = Lower(StringView(tag).substr(prefix.size()));
            break;
        }

        if (model.empty())
        {
            const String stem = ShaderStem(shaderName);
            if (stem == "pbribl")
                model = "standard";
            else if (stem == "unlit")
                model = "unlit";
            else if (stem == "toon")
                model = "toon";
            else
            {
                result.Route = MaterialRenderRoute::Unsupported;
                return result;
            }
        }

        if (model == "standard")
            result.Model = MaterialModel::Standard;
        else if (model == "unlit")
            result.Model = MaterialModel::Unlit;
        else if (model == "toon")
            result.Model = MaterialModel::Toon;
        else if (model == "custom")
            result.Route = MaterialRenderRoute::ForwardOnly;
        else
            result.Route = MaterialRenderRoute::Unsupported;
        return result;
    }

    MaterialRenderClassification MaterialRenderClassifier::Classify(const Material& material)
    {
        const AssetHandle<Shader> shader = material.GetShader();
        if (!shader)
            return Classify({}, {}, false, false);

        const Ref<ShaderTechnique>& technique = shader->GetTechnique(material.GetVariation());
        bool hasBlending = false;
        if (technique)
        {
            hasBlending = std::any_of(technique->GetRenderPasses().begin(), technique->GetRenderPasses().end(),
                                      [](const Ref<ShaderRenderPass>& pass) { return pass && pass->HasBlending(); });
        }
        const bool alphaMasked = material.GetVariation().Has("ALPHA_MASK") && material.GetVariation().GetBool("ALPHA_MASK");
        static const Vector<String> emptyTags;
        const Vector<String>& tags = technique ? technique->GetTags() : emptyTags;
        const bool hasExplicitMaterialModel =
          std::any_of(tags.begin(), tags.end(), [](const String& tag) { return tag.starts_with("material_model="); });
        MaterialRenderClassification result;
        if (hasExplicitMaterialModel)
            result = Classify({}, tags, hasBlending, alphaMasked);
        else
        {
            String shaderIdentity = shader->GetName();
            if (shaderIdentity.empty())
            {
                Path shaderPath;
                if (AssetManager::TryGet() != nullptr && AssetManager::TryGet()->GetAssetPath(shader.GetUUID(), shaderPath))
                    shaderIdentity = shaderPath.generic_string();
            }
            result = Classify(shaderIdentity, tags, hasBlending, alphaMasked);
        }
        if (material.HasAlphaModeOverride())
            result.Alpha = material.GetAlphaMode();
        return result;
    }

    GpuMaterialData GpuMaterialPacker::Pack(const StandardMaterialDesc& desc)
    {
        GpuMaterialData output;
        output.BaseColor = glm::max(desc.BaseColor, glm::vec4(0.0f));
        output.EmissiveAlphaCutoff = { glm::max(desc.Emissive, glm::vec3(0.0f)) * std::max(desc.EmissiveIntensity, 0.0f),
                                       std::clamp(desc.AlphaCutoff, 0.0f, 1.0f) };
        output.MetallicRoughnessNormalAo = { std::clamp(desc.Metallic, 0.0f, 1.0f), std::clamp(desc.Roughness, 0.045f, 1.0f),
                                             std::max(desc.NormalScale, 0.0f), std::clamp(desc.AmbientOcclusion, 0.0f, 1.0f) };
        output.TextureIndices0 = { desc.BaseColorTexture, desc.NormalTexture, desc.MetallicRoughnessTexture, desc.AmbientOcclusionTexture };
        output.TextureIndices1 = { desc.EmissiveTexture, desc.SamplerIndex, static_cast<uint32_t>(desc.Flags),
                                   PackModelAndAlpha(desc.Model, desc.Alpha) };
        output.ToonShadowBands = { glm::max(desc.ToonShadowColor, glm::vec3(0.0f)), std::clamp(desc.ToonBands, 2.0f, 16.0f) };
        output.ToonSpecular = { glm::max(desc.ToonSpecularColor, glm::vec3(0.0f)), std::clamp(desc.ToonSpecularThreshold, 0.0f, 1.0f) };
        output.ToonRim = { glm::max(desc.ToonRimColor, glm::vec3(0.0f)), std::clamp(desc.ToonRimThreshold, 0.0f, 1.0f) };
        output.ToonControls = { std::clamp(desc.ToonBandSmoothness, 0.0f, 0.5f), std::clamp(desc.ToonSpecularSmoothness, 0.0f, 0.5f),
                                std::max(desc.ToonSpecularStrength, 0.0f), std::clamp(desc.ToonRimSmoothness, 0.0f, 0.5f) };
        output.ToonArtistic = { std::max(desc.ToonRimPower, 0.01f), std::max(desc.ToonRimStrength, 0.0f),
                                std::clamp(desc.ToonRimShadowMask, 0.0f, 1.0f), std::max(desc.ToonIndirectStrength, 0.0f) };
        output.ToonPattern = { std::max(desc.ToonPatternScale, 0.001f), std::clamp(desc.ToonPatternStrength, 0.0f, 1.0f),
                               std::clamp(desc.ToonPatternSmoothness, 0.0f, 0.5f), std::max(desc.ToonPatternDistanceFade, 0.0f) };
        output.ToonOutlineColor = glm::max(desc.ToonOutlineColor, glm::vec4(0.0f));
        output.ToonOutline = { std::max(desc.ToonOutlineWidth, 0.0f), std::max(desc.ToonOutlineDepthThreshold, 0.0f),
                               std::clamp(desc.ToonOutlineNormalThreshold, 0.0f, 1.0f), std::max(desc.ToonOutlineDistanceFade, 0.0f) };
        output.ToonSilhouette.x = std::max(desc.ToonSilhouetteWidth, 0.0f);
        output.ToonStyle = { std::clamp(desc.ToonRampStrength, 0.0f, 1.0f), std::clamp(desc.ToonRampOffset, -1.0f, 1.0f),
                             std::clamp(desc.ToonMatcapStrength, 0.0f, 1.0f), desc.ToonMatcapRotation };
        output.TextureIndices2 = { desc.ToonPatternTexture, desc.ToonRampTexture, desc.ToonMatcapTexture,
                                   static_cast<uint32_t>(desc.ToonPatternMappingMode) };
        return output;
    }

    GpuMaterialData GpuMaterialPacker::PackUnsupported()
    {
        GpuMaterialData output;
        output.TextureIndices1.w = UnsupportedModelAndAlpha;
        return output;
    }
} // namespace Crowny

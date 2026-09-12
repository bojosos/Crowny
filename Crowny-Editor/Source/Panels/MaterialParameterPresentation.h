#pragma once

#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/MaterialPreset.h"

namespace Crowny
{
    inline bool IsMaterialTextureParameter(ShaderParamType type)
    {
        return type == ShaderParamType::Texture2D || type == ShaderParamType::Texture3D || type == ShaderParamType::TextureCube;
    }

    inline const char* MaterialParameterGroup(const ShaderParameterDesc& parameter)
    {
        if (IsMaterialTextureParameter(parameter.Type))
            return "Textures";
        const String& name = parameter.Identifier;
        if (parameter.BlockName == "OutlineParams" || name.find("Outline") != String::npos || name == "thickness" || name == "toonSilhouetteWidth")
            return "Outline";
        if (name.starts_with("emissive"))
            return "Emission";
        if (name.starts_with("alpha") || name == "opacity")
            return "Transparency";
        if (parameter.BlockName == "ToonParams")
            return "Toon shading";
        return "Surface";
    }

    inline String MaterialParameterTooltip(const ShaderParameterDesc& parameter)
    {
        static const UnorderedMap<String, String> descriptions = {
            { "albedo", "Base surface color. Multiplies the albedo texture." },
            { "tint", "Surface tint. Multiplies the albedo texture and mesh vertex colors." },
            { "roughness", "Controls reflection sharpness. Lower values look polished; higher values spread the reflection." },
            { "metalness", "Use 0 for nonmetals and 1 for bare metal. A metal's base color also colors its reflections." },
            { "albedoMap", "Base color texture. Multiplied by the surface color or tint." },
            { "normalMap", "Tangent-space normal texture. Adds surface detail without changing the mesh silhouette." },
            { "roughnessMap", "Texture that varies roughness across the surface." },
            { "metallicMap", "Texture that marks metallic and nonmetallic parts of the surface." },
            { "aoMap", "Ambient occlusion texture. Darkens indirect light in creases and recesses." },
            { "emissive", "Color of light emitted by the surface." },
            { "emissiveMap", "Texture that controls which parts of the surface emit light." },
            { "emissiveIntensity", "Brightness multiplier for the emission color and texture." },
            { "alphaCutoff", "Pixels below this opacity are discarded when Alpha Mask is selected." },
            { "bands", "Number of lighting bands used to shade the Toon material." },
            { "specularSize", "Size of the Toon specular highlight." },
            { "specularSmoothness", "Softness of the transition around the specular highlight." },
            { "rimStrength", "Strength of the light around the silhouette." },
            { "rimPower", "Controls how narrowly rim lighting follows the silhouette." },
            { "rimThreshold", "Threshold at which rim lighting appears along the silhouette." },
            { "shadowBrightness", "Minimum brightness of the Toon lighting bands." },
            { "toonShadowColor", "Tint applied to the shadowed part of the Toon surface." },
            { "toonSpecularColor", "Color of the Toon specular highlight." },
            { "toonRimColor", "Color of the light around the silhouette." },
            { "toonBandSmoothness", "Softness of the transitions between lighting bands." },
            { "toonSpecularThreshold", "Lighting threshold required for the specular highlight." },
            { "toonSpecularSmoothness", "Softness of the specular highlight's edge." },
            { "toonSpecularStrength", "Brightness of the specular highlight." },
            { "toonRimSmoothness", "Softness of the rim light's edge." },
            { "toonRimStrength", "Brightness of the light around the silhouette." },
            { "toonRimShadowMask", "Controls how strongly shadows suppress rim lighting." },
            { "toonIndirectStrength", "Strength of indirect environment lighting on the Toon surface." },
            { "toonPatternScale", "Scale of the hatching or scratch pattern." },
            { "toonPatternStrength", "Amount of hatching or scratch pattern blended into the surface." },
            { "toonPatternSmoothness", "Softness of the hatching pattern's edges." },
            { "toonPatternDistanceFade", "Distance over which the pattern fades." },
            { "toonPatternMapping", "Pattern coordinates: 0 = mesh UVs, 1 = triplanar, 2 = screen, 3 = procedural hatching." },
            { "toonRampStrength", "Amount of the diffuse ramp texture used to shape lighting." },
            { "toonRampOffset", "Shifts the lighting lookup along the diffuse ramp texture." },
            { "toonMatcapStrength", "Amount of the matcap texture blended into the surface." },
            { "toonMatcapRotation", "Rotation of the matcap texture, in radians." },
            { "toonPatternTexture", "Hatching or scratch texture. Enable it with Pattern Strength." },
            { "toonRampTexture", "Color ramp used to shape diffuse lighting. Enable it with Ramp Strength." },
            { "toonMatcapTexture", "Texture of a lit sphere used for matcap shading. Enable it with Matcap Strength." },
            { "outlineColor", "Color of the Toon outline." },
            { "thickness", "Outline thickness for shaders that use a screen-space outline." },
            { "toonSilhouetteWidth", "Width of the inverted-hull outline around the mesh." },
            { "toonOutlineDepthThreshold", "Depth difference required to draw an outline between neighboring pixels." },
            { "toonOutlineNormalThreshold", "Surface normal difference required to draw an internal outline." },
            { "toonOutlineDistanceFade", "Distance over which Toon outlines fade." }
        };
        const auto found = descriptions.find(parameter.Identifier);
        String text = found != descriptions.end() ? found->second : "Shader parameter: " + parameter.Identifier + ".";
        if (parameter.HasRange)
            text += "\nCtrl+click the value to type an exact number.";
        text += "\nRight-click to restore the shader default.";
        return text;
    }

    inline void ResetMaterialParameter(Material& material, const Material& defaults, const ShaderParameterDesc& parameter)
    {
        switch (parameter.Type)
        {
        case ShaderParamType::Int2:
            material.SetInt2(parameter.Identifier, defaults.GetDataParam<glm::ivec2>(parameter.Identifier));
            return;
        case ShaderParamType::Int3:
            material.SetInt3(parameter.Identifier, defaults.GetDataParam<glm::ivec3>(parameter.Identifier));
            return;
        case ShaderParamType::Int4:
            material.SetInt4(parameter.Identifier, defaults.GetDataParam<glm::ivec4>(parameter.Identifier));
            return;
        case ShaderParamType::Mat3:
            material.SetMat3(parameter.Identifier, defaults.GetDataParam<glm::mat3>(parameter.Identifier));
            return;
        case ShaderParamType::Mat4:
            material.SetMatrix(parameter.Identifier, defaults.GetDataParam<glm::mat4>(parameter.Identifier));
            return;
        default:
            break;
        }
        if (IsMaterialTextureParameter(parameter.Type))
        {
            material.SetTexture(parameter.Identifier, AssetHandle<Texture>{});
            return;
        }
        const Ref<MaterialPreset> preset = MaterialPreset::CaptureFromMaterial(defaults, "Shader default");
        Vector<String> otherParameters;
        for (const auto& value : preset->GetParameters())
            if (value.Name != parameter.Identifier)
                otherParameters.push_back(value.Name);
        for (const String& name : otherParameters)
            preset->Remove(name);
        material.ApplyPreset(*preset);
    }
} // namespace Crowny

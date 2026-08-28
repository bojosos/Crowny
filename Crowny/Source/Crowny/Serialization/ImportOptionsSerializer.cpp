#include "cwpch.h"

#include "Crowny/Serialization/ImportOptionsSerializer.h"

#include "Crowny/Audio/AudioClip.h"
#include "Crowny/Import/ImportOptions.h"

#include "Crowny/Common/Yaml.h"

namespace Crowny
{

    void ImportOptionsSerializer::Serialize(YAML::Emitter& out, const Ref<ImportOptions>& importOptions)
    {
        switch (importOptions->GetImportOptionsType())
        {
        case ImportOptionsType::None:
            break;
        case ImportOptionsType::AudioClip: {
            Ref<AudioClipImportOptions> audioImportOptions = StaticRefCast<AudioClipImportOptions>(importOptions);
            BeginYAMLMap(out, "AudioImporter");

            SerializeEnumYAML(out, "Format", audioImportOptions->Format);
            SerializeEnumYAML(out, "ReadMode", audioImportOptions->ReadMode);
            SerializeValueYAML(out, "Is3D", audioImportOptions->Is3D);
            SerializeValueYAML(out, "BitDepth", audioImportOptions->BitDepth);
            SerializeValueYAML(out, "Quality", audioImportOptions->Quality);

            EndYAMLMap(out, "AudioImporter");
            break;
        }
        case ImportOptionsType::Shader: {
            Ref<ShaderImportOptions> shaderImportOptions = StaticRefCast<ShaderImportOptions>(importOptions);
            BeginYAMLMap(out, "ShaderImporter");

            SerializeEnumYAML(out, "Language", shaderImportOptions->Language);
            BeginYAMLMap(out, "Defines");
            Map<String, String> sortedDefines(shaderImportOptions->GetDefines().begin(), shaderImportOptions->GetDefines().end());
            for (const auto& [name, value] : sortedDefines)
                SerializeValueYAML(out, name.c_str(), value);
            EndYAMLMap(out, "Defines");

            EndYAMLMap(out, "ShaderImporter");
            break;
        }
        case ImportOptionsType::Texture: {
            Ref<TextureImportOptions> textureImportOptions = StaticRefCast<TextureImportOptions>(importOptions);
            BeginYAMLMap(out, "TextureImporter");

            SerializeEnumYAML(out, "Format", textureImportOptions->Format);
            SerializeEnumYAML(out, "Shape", textureImportOptions->Shape);
            SerializeEnumYAML(out, "DiskFormat", textureImportOptions->DiskFormat);

            SerializeValueYAML(out, "AutoFormat", textureImportOptions->AutomaticFormat);
            SerializeValueYAML(out, "CpuCached", textureImportOptions->CpuCached);
            SerializeValueYAML(out, "GenerateMips", textureImportOptions->GenerateMips);
            SerializeValueYAML(out, "MaxMip", textureImportOptions->MaxMip);
            SerializeEnumYAML(out, "MipFilter", textureImportOptions->MipFilter);
            SerializeEnumYAML(out, "MipMode", textureImportOptions->MipMode);
            SerializeValueYAML(out, "MipWrap", textureImportOptions->MipWrap);
            SerializeValueYAML(out, "PreserveAlphaCoverage", textureImportOptions->PreserveAlphaCoverage);
            SerializeValueYAML(out, "AlphaCutoff", textureImportOptions->AlphaCutoff);
            SerializeValueYAML(out, "sRGB", textureImportOptions->SRGB);

            EndYAMLMap(out, "TextureImporter");
            break;
        }
        case ImportOptionsType::Script: {
            Ref<CSharpScriptImportOptions> scriptImportOptions = StaticRefCast<CSharpScriptImportOptions>(importOptions);
            BeginYAMLMap(out, "ScriptImporter");

            SerializeValueYAML(out, "IsEditorScript", scriptImportOptions->IsEditorScript);

            EndYAMLMap(out, "ScriptImporter");
            break;
        }
        case ImportOptionsType::Mesh: {
            Ref<MeshImportOptions> meshImportOptions = StaticRefCast<MeshImportOptions>(importOptions);
            BeginYAMLMap(out, "MeshImporter");

            SerializeValueYAML(out, "Optimize", meshImportOptions->Optimize);
            SerializeValueYAML(out, "Compress", meshImportOptions->Compress);
            SerializeValueYAML(out, "KeepQuads", meshImportOptions->KeepQuads);
            SerializeValueYAML(out, "ScaleFactor", meshImportOptions->ScaleFactor);
            SerializeValueYAML(out, "CpuCached", meshImportOptions->CpuCached);
            SerializeValueYAML(out, "SmoothNormals", meshImportOptions->SmoothNormals);
            SerializeValueYAML(out, "SmoothingAngle", meshImportOptions->SmoothingAngle);
            SerializeEnumYAML(out, "Normals", meshImportOptions->NormalsMode);
            SerializeEnumYAML(out, "Tangents", meshImportOptions->TangentsMode);
            SerializeEnumYAML(out, "IndexFormat", meshImportOptions->IndexFormat);
            SerializeValueYAML(out, "ImportAnimations", meshImportOptions->ImportAnimations);
            SerializeValueYAML(out, "ImportMorphMeshes", meshImportOptions->ImportMorphMeshes);
            SerializeValueYAML(out, "ImportBones", meshImportOptions->ImportBones);
            SerializeValueYAML(out, "ImportRootMotion", meshImportOptions->ImportRootMotion);
            SerializeValueYAML(out, "ImportMaterials", meshImportOptions->ImportMaterials);
            SerializeValueYAML(out, "ImportVertexColors", meshImportOptions->ImportVertexColors);
            SerializeValueYAML(out, "FlipUVs", meshImportOptions->FlipUVs);
            SerializeValueYAML(out, "FlipWindingOrder", meshImportOptions->FlipWindingOrder);
            SerializeValueYAML(out, "GenerateMeshlets", meshImportOptions->GenerateMeshlets);
            SerializeValueYAML(out, "GenerateLods", meshImportOptions->GenerateLods);
            SerializeValueYAML(out, "LodCount", meshImportOptions->LodCount);

            out << YAML::Key << "AnimationClips" << YAML::Value << YAML::BeginSeq;
            for (const ExtraAnimationClipInfo& clip : meshImportOptions->AnimationInfo)
            {
                out << YAML::BeginMap;
                SerializeValueYAML(out, "Name", clip.Name);
                SerializeValueYAML(out, "StartFrame", clip.StartFrame);
                SerializeValueYAML(out, "EndFrame", clip.EndFrame);
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;

            EndYAMLMap(out, "MeshImporter");
            break;
        }
        case ImportOptionsType::Font: {
            Ref<FontImportOptions> fontImportOptions = StaticRefCast<FontImportOptions>(importOptions);
            BeginYAMLMap(out, "FontImporter");

            SerializeEnumYAML(out, "AtlasDimensionsConstraint", fontImportOptions->AtlasDimensionsConstraint);
            SerializeValueYAML(out, "GetKerningData", fontImportOptions->GetKerningData);
            SerializeValueYAML(out, "AutomaticFontSampling", fontImportOptions->AutomaticFontSampling);
            SerializeValueYAML(out, "SampingFontSize", fontImportOptions->SamplingFontSize);
            SerializeValueYAML(out, "AutoSizeAtlas", fontImportOptions->AutoSizeAtlas);
            SerializeValueYAML(out, "AtlasWidth", fontImportOptions->AtlasWidth);
            SerializeValueYAML(out, "AtlasHeight", fontImportOptions->AtlasHeight);
            SerializeEnumYAML(out, "Range", fontImportOptions->Range);
            SerializeValueYAML(out, "CustomCharset", fontImportOptions->CustomCharset);
            SerializeValueYAML(out, "Padding", fontImportOptions->Padding);
            SerializeValueYAML(out, "DynamicFontAtlas", fontImportOptions->DynamicFontAtlas);
            SerializeValueYAML(out, "BoldWeight", fontImportOptions->BoldWeight);
            SerializeValueYAML(out, "BoldSpacing", fontImportOptions->BoldSpacing);
            SerializeValueYAML(out, "TabMultiple", fontImportOptions->TabMultiple);
            SerializeValueYAML(out, "ItalicStyle", fontImportOptions->ItalicStyle);
            out << YAML::Key << "FallbackFonts" << YAML::Value << YAML::BeginSeq;
            Set<UUID> serializedFallbacks;
            for (const UUID& fallback : fontImportOptions->FallbackFonts)
            {
                if (serializedFallbacks.size() == Font::MAX_FALLBACK_FONTS)
                    break;
                if (!fallback.Empty() && serializedFallbacks.insert(fallback).second)
                    out << fallback;
            }
            out << YAML::EndSeq;

            EndYAMLMap(out, "FontImporter");
            break;
        }
        }
    }

    Ref<ImportOptions> ImportOptionsSerializer::Deserialize(const YAML::Node& data)
    {
        if (const YAML::Node& audioImportOptionsNode = data["AudioImporter"])
        {
            Ref<AudioClipImportOptions> audioImportOptions = CreateRef<AudioClipImportOptions>();

            DeserializeEnumYAML(audioImportOptionsNode, "Format", audioImportOptions->Format, AudioFormat::VORBIS,
                                "Audio format \'{0}\' in metadata file is invalid.", 0, 2);
            DeserializeEnumYAML(audioImportOptionsNode, "ReadMode", audioImportOptions->ReadMode, AudioReadMode::LoadCompressed,
                                "Audio read mode \'{0}\' in metadata file is invalid.", 0, 3);
            DeserializeValueYAML(audioImportOptionsNode, "Is3D", audioImportOptions->Is3D, true);
            DeserializeValueYAML(audioImportOptionsNode, "Quality", audioImportOptions->Quality, 1.0f,
                                 "Audio quality  \'{0}\' in metadata file is invalid.", 0.0f, 1.0f);

            uint32_t bitDepth = audioImportOptionsNode["BitDepth"].as<uint32_t>(8);
            if (bitDepth != 8 && bitDepth != 16 && bitDepth != 24 && bitDepth != 32)
                CW_ENGINE_WARN("Bit depth \'{0}\' in metadata file is invalid.", bitDepth);
            audioImportOptions->BitDepth = bitDepth;

            return audioImportOptions;
        }
        else if (const YAML::Node& textureImportOptionsNode = data["TextureImporter"])
        {
            Ref<TextureImportOptions> textureImportOptions = CreateRef<TextureImportOptions>();

            DeserializeEnumYAML(textureImportOptionsNode, "Format", textureImportOptions->Format, TextureFormat::RGBA8,
                                "Texture format \'{}\' in metadata file is invalid.", 0, (int32_t)TextureFormat::FormatCount);
            DeserializeEnumYAML(textureImportOptionsNode, "Shape", textureImportOptions->Shape, TextureShape::TEXTURE_2D,
                                "Texture shape \'{}\' in metadata file is invalid.", 0, 4);
            DeserializeEnumYAML(textureImportOptionsNode, "DiskFormat", textureImportOptions->DiskFormat, TextureDiskFormat::UASTC,
                                "Texture disk format \'{}\' in metadata file is invalid.", 0,
                                static_cast<int32_t>(TextureDiskFormat::Count));
            DeserializeValueYAML(textureImportOptionsNode, "AutoFormat", textureImportOptions->AutomaticFormat, true);
            DeserializeValueYAML(textureImportOptionsNode, "GenerateMips", textureImportOptions->GenerateMips, true);
            DeserializeValueYAML(textureImportOptionsNode, "CpuCached", textureImportOptions->CpuCached, false);
            DeserializeValueYAML(textureImportOptionsNode, "sRGB", textureImportOptions->SRGB, true);
            DeserializeValueYAML(textureImportOptionsNode, "MaxMip", textureImportOptions->MaxMip, 0U);
            DeserializeEnumYAML(textureImportOptionsNode, "MipFilter", textureImportOptions->MipFilter, TextureMipFilter::Kaiser,
                                "Texture mip filter '{}' in metadata file is invalid.", 0,
                                static_cast<int32_t>(TextureMipFilter::Count));
            DeserializeEnumYAML(textureImportOptionsNode, "MipMode", textureImportOptions->MipMode, TextureMipMode::Color,
                                "Texture mip mode '{}' in metadata file is invalid.", 0,
                                static_cast<int32_t>(TextureMipMode::Count));
            DeserializeValueYAML(textureImportOptionsNode, "MipWrap", textureImportOptions->MipWrap, false);
            DeserializeValueYAML(textureImportOptionsNode, "PreserveAlphaCoverage", textureImportOptions->PreserveAlphaCoverage, false);
            DeserializeValueYAML(textureImportOptionsNode, "AlphaCutoff", textureImportOptions->AlphaCutoff, 0.5f);

            return textureImportOptions;
        }
        else if (const auto& shaderImportOptionsNode = data["ShaderImporter"])
        {
            Ref<ShaderImportOptions> importOptions = CreateRef<ShaderImportOptions>();
            const uint32_t language = shaderImportOptionsNode["Language"].as<uint32_t>(static_cast<uint32_t>(ShaderLanguage::VKSL));
            switch (static_cast<ShaderLanguage>(language))
            {
            case ShaderLanguage::VKSL:
            case ShaderLanguage::GLSL:
            case ShaderLanguage::HLSL:
            case ShaderLanguage::MSL:
                importOptions->Language = static_cast<ShaderLanguage>(language);
                break;
            default:
                CW_ENGINE_WARN("Shader language '{}' in metadata is invalid. Using VKSL.", language);
                break;
            }

            const YAML::Node defines = shaderImportOptionsNode["Defines"];
            if (defines && defines.IsMap())
            {
                for (const auto& define : defines)
                {
                    const String name = define.first.as<String>();
                    if (!ShaderSourceParser::IsIdentifier(name) || !define.second.IsScalar())
                    {
                        CW_ENGINE_WARN("Ignoring invalid shader define '{}' in metadata.", name);
                        continue;
                    }
                    importOptions->SetDefine(name, define.second.as<String>());
                }
            }
            return importOptions;
        }
        else if (const auto& scriptImportOptionsNode = data["ScriptImporter"])
        {
            Ref<CSharpScriptImportOptions> scriptImportOptions = CreateRef<CSharpScriptImportOptions>();
            DeserializeValueYAML(scriptImportOptionsNode, "IsEditorScript", scriptImportOptions->IsEditorScript, false);
            return scriptImportOptions;
        }
        else if (const auto& fontImportOptionsNode = data["FontImporter"])
        {
            Ref<FontImportOptions> fontImportOptions = CreateRef<FontImportOptions>();

            DeserializeEnumYAML(fontImportOptionsNode, "AtlasDimensionsConstraint", fontImportOptions->AtlasDimensionsConstraint,
                                Font::AtlasDimensionsConstraint::POWER_OF_TWO_SQUARE,
                                "Atlas dimension constraints \'{}\' in metadata file is invalid.", 0, Font::AtlasDimensionsConstraint::COUNT);
            DeserializeValueYAML(fontImportOptionsNode, "GetKerningData", fontImportOptions->GetKerningData, true);
            DeserializeValueYAML(fontImportOptionsNode, "AutomaticFontSampling", fontImportOptions->AutomaticFontSampling, true);
            DeserializeValueYAML(fontImportOptionsNode, "SampingFontSize", fontImportOptions->SamplingFontSize, 64U);
            DeserializeValueYAML(fontImportOptionsNode, "AutoSizeAtlas", fontImportOptions->AutoSizeAtlas, false);
            DeserializeValueYAML(fontImportOptionsNode, "AtlasWidth", fontImportOptions->AtlasWidth, 1024U);
            DeserializeValueYAML(fontImportOptionsNode, "AtlasHeight", fontImportOptions->AtlasHeight, 1024U);
            DeserializeEnumYAML(fontImportOptionsNode, "Range", fontImportOptions->Range, CharsetRange::ASCII,
                                "Charset range \'{}\' in metadata file is invalid.", 0, CharsetRange::Count);
            DeserializeValueYAML(fontImportOptionsNode, "CustomCharset", fontImportOptions->CustomCharset, String());
            DeserializeValueYAML(fontImportOptionsNode, "Padding", fontImportOptions->Padding, 0U);
            DeserializeValueYAML(fontImportOptionsNode, "DynamicFontAtlas", fontImportOptions->DynamicFontAtlas, false);
            DeserializeValueYAML(fontImportOptionsNode, "BoldWeight", fontImportOptions->BoldWeight, 0.75f);
            DeserializeValueYAML(fontImportOptionsNode, "BoldSpacing", fontImportOptions->BoldSpacing, 7.0f);
            DeserializeValueYAML(fontImportOptionsNode, "TabMultiple", fontImportOptions->TabMultiple, 4U);
            DeserializeValueYAML(fontImportOptionsNode, "ItalicStyle", fontImportOptions->ItalicStyle, 35U);
            if (const YAML::Node fallbackFonts = fontImportOptionsNode["FallbackFonts"]; fallbackFonts && fallbackFonts.IsSequence())
            {
                for (const YAML::Node fallback : fallbackFonts)
                {
                    const UUID fallbackId = fallback.as<UUID>(UUID::EMPTY);
                    if (fallbackId.Empty() || std::find(fontImportOptions->FallbackFonts.begin(), fontImportOptions->FallbackFonts.end(),
                                                        fallbackId) != fontImportOptions->FallbackFonts.end())
                        continue;
                    fontImportOptions->FallbackFonts.push_back(fallbackId);
                    if (fontImportOptions->FallbackFonts.size() == Font::MAX_FALLBACK_FONTS)
                        break;
                }
            }

            return fontImportOptions;
        }
        else if (const YAML::Node& meshImportOptionsNode = data["MeshImporter"])
        {
            Ref<MeshImportOptions> meshImportOptions = CreateRef<MeshImportOptions>();

            DeserializeEnumYAML(meshImportOptionsNode, "Normals", meshImportOptions->NormalsMode, NormalsImportMode::Import,
                                "Normals import mode \'{}\' in metadata file is invalid.", 0, (int32_t)NormalsImportMode::Count);
            DeserializeEnumYAML(meshImportOptionsNode, "Tangents", meshImportOptions->TangentsMode, NormalsImportMode::Import,
                                "Normals import mode \'{}\' in metadata file is invalid.", 0, (int32_t)NormalsImportMode::Count);
            DeserializeEnumYAML(meshImportOptionsNode, "IndexFormat", meshImportOptions->IndexFormat, MeshIndexFormat::Auto,
                                "Index type \'{}\' in metadata file is invalid.", 0, (int32_t)MeshIndexFormat::Count);
            DeserializeValueYAML(meshImportOptionsNode, "Compress", meshImportOptions->Compress, false);
            DeserializeValueYAML(meshImportOptionsNode, "Optimize", meshImportOptions->Optimize, false);
            DeserializeValueYAML(meshImportOptionsNode, "KeepQuads", meshImportOptions->KeepQuads, false);
            DeserializeValueYAML(meshImportOptionsNode, "ScaleFactor", meshImportOptions->ScaleFactor, 1.0f);
            DeserializeValueYAML(meshImportOptionsNode, "CpuCached", meshImportOptions->CpuCached, false);
            DeserializeValueYAML(meshImportOptionsNode, "SmoothNormals", meshImportOptions->SmoothNormals, false);
            DeserializeValueYAML(meshImportOptionsNode, "SmoothingAngle", meshImportOptions->SmoothingAngle, 175.0f);
            DeserializeValueYAML(meshImportOptionsNode, "ImportAnimations", meshImportOptions->ImportAnimations, false);
            DeserializeValueYAML(meshImportOptionsNode, "ImportMorphMeshes", meshImportOptions->ImportMorphMeshes, false);
            DeserializeValueYAML(meshImportOptionsNode, "ImportBones", meshImportOptions->ImportBones, false);
            DeserializeValueYAML(meshImportOptionsNode, "ImportRootMotion", meshImportOptions->ImportRootMotion, false);
            DeserializeValueYAML(meshImportOptionsNode, "ImportMaterials", meshImportOptions->ImportMaterials, true);
            DeserializeValueYAML(meshImportOptionsNode, "ImportVertexColors", meshImportOptions->ImportVertexColors, true);
            DeserializeValueYAML(meshImportOptionsNode, "FlipUVs", meshImportOptions->FlipUVs, false);
            DeserializeValueYAML(meshImportOptionsNode, "FlipWindingOrder", meshImportOptions->FlipWindingOrder, false);
            DeserializeValueYAML(meshImportOptionsNode, "GenerateMeshlets", meshImportOptions->GenerateMeshlets, true);
            DeserializeValueYAML(meshImportOptionsNode, "GenerateLods", meshImportOptions->GenerateLods, true);
            DeserializeValueYAML(meshImportOptionsNode, "LodCount", meshImportOptions->LodCount, 4u);
            meshImportOptions->LodCount = std::clamp(meshImportOptions->LodCount, 1u, 16u);

            const YAML::Node animationClips = meshImportOptionsNode["AnimationClips"];
            if (animationClips && animationClips.IsSequence())
            {
                for (const YAML::Node& clipNode : animationClips)
                {
                    ExtraAnimationClipInfo clip;
                    DeserializeValueYAML(clipNode, "Name", clip.Name, String());
                    DeserializeValueYAML(clipNode, "StartFrame", clip.StartFrame, 0U);
                    DeserializeValueYAML(clipNode, "EndFrame", clip.EndFrame, 0U);
                    if (!clip.Name.empty() && clip.EndFrame >= clip.StartFrame)
                        meshImportOptions->AnimationInfo.push_back(std::move(clip));
                    else
                        CW_ENGINE_WARN("Ignoring invalid mesh animation clip metadata.");
                }
            }

            return meshImportOptions;
        }
        else
        {
            CW_ENGINE_WARN("Metadata file does not have valid import options. Correspnding asset may be broken.");
            return CreateRef<ImportOptions>();
        }
        return nullptr;
    }
} // namespace Crowny

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
            Ref<AudioClipImportOptions> audioImportOptions =
              std::static_pointer_cast<AudioClipImportOptions>(importOptions);
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
            Ref<ShaderImportOptions> shaderImportOptions = std::static_pointer_cast<ShaderImportOptions>(importOptions);
            BeginYAMLMap(out, "ShaderImporter");

            EndYAMLMap(out, "ShaderImporter");
            break;
        }
        case ImportOptionsType::Texture: {
            Ref<TextureImportOptions> textureImportOptions =
              std::static_pointer_cast<TextureImportOptions>(importOptions);
            BeginYAMLMap(out, "TextureImporter");

            SerializeEnumYAML(out, "Format", textureImportOptions->Format);
            SerializeEnumYAML(out, "Shape", textureImportOptions->Shape);

            SerializeValueYAML(out, "AutoFormat", textureImportOptions->AutomaticFormat);
            SerializeValueYAML(out, "CpuCached", textureImportOptions->CpuCached);
            SerializeValueYAML(out, "GenerateMips", textureImportOptions->GenerateMips);
            SerializeValueYAML(out, "MaxMip", textureImportOptions->MaxMip);
            SerializeValueYAML(out, "sRGB", textureImportOptions->SRGB);

            EndYAMLMap(out, "TextureImporter");
            break;
        }
        case ImportOptionsType::Script: {
            Ref<CSharpScriptImportOptions> scriptImportOptions =
              std::static_pointer_cast<CSharpScriptImportOptions>(importOptions);
            BeginYAMLMap(out, "ScriptImporter");

            SerializeValueYAML(out, "IsEditorScript", scriptImportOptions->IsEditorScript);

            EndYAMLMap(out, "ScriptImporter");
            break;
        }
        case ImportOptionsType::Font: {
            Ref<FontImportOptions> fontImportOptions = std::static_pointer_cast<FontImportOptions>(importOptions);
            BeginYAMLMap(out, "FontImporter");

            SerializeEnumYAML(out, "AtlasDimensionsConstraint", fontImportOptions->AtlasDimensionsConstraint);
            SerializeValueYAML(out, "GetKerningData", fontImportOptions->GetKerningData);
            SerializeValueYAML(out, "AutomaticFontSampling", fontImportOptions->AutomaticFontSampling);
            SerializeValueYAML(out, "SampingFontSize", fontImportOptions->SampingFontSize);
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

            DeserializeEnumYAML(audioImportOptionsNode, "ReadMode", audioImportOptions->Format, AudioFormat::VORBIS,
                                "Audio format \'{0}\' in metadata file is invalid.", 0, 2);
            DeserializeEnumYAML(audioImportOptionsNode, "ReadMode", audioImportOptions->ReadMode,
                                AudioReadMode::LoadCompressed, "Audio read mode \'{0}\' in metadata file is invalid.",
                                0, 3);
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
                                "Texture format \'{}\' in metadata file is invalid.", 0,
                                (int32_t)TextureFormat::FormatCount);
            DeserializeEnumYAML(textureImportOptionsNode, "Shape", textureImportOptions->Shape,
                                TextureShape::TEXTURE_2D, "Texture shape \'{}\' in metadata file is invalid.", 0, 4);
            DeserializeValueYAML(textureImportOptionsNode, "AutoFormat", textureImportOptions->AutomaticFormat, true);
            DeserializeValueYAML(textureImportOptionsNode, "GenerateMips", textureImportOptions->GenerateMips, false);
            DeserializeValueYAML(textureImportOptionsNode, "CpuCached", textureImportOptions->CpuCached, false);
            DeserializeValueYAML(textureImportOptionsNode, "sRGB", textureImportOptions->SRGB, false);
            DeserializeValueYAML(textureImportOptionsNode, "MaxMip", textureImportOptions->MaxMip, 0U);

            return textureImportOptions;
        }
        else if (const auto& shaderImportOptionsNode = data["ShaderImporter"])
        {
            Ref<ShaderImportOptions> importOptions = CreateRef<ShaderImportOptions>();
            // TODO: Deserialize defines
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

            DeserializeEnumYAML(fontImportOptionsNode, "AtlasDimensionsConstraint",
                                fontImportOptions->AtlasDimensionsConstraint,
                                Font::AtlasDimensionsConstraint::POWER_OF_TWO_SQUARE,
                                "Atlas dimension constraints \'{}\' in metadata file is invalid.", 0,
                                Font::AtlasDimensionsConstraint::COUNT);
            DeserializeValueYAML(fontImportOptionsNode, "GetKerningData", fontImportOptions->GetKerningData, true);
            DeserializeValueYAML(fontImportOptionsNode, "AutomaticFontSampling",
                                 fontImportOptions->AutomaticFontSampling, true);
            DeserializeValueYAML(fontImportOptionsNode, "SampingFontSize", fontImportOptions->SampingFontSize, 64U);
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
            DeserializeValueYAML(fontImportOptionsNode, "TabMultiple", fontImportOptions->TabMultiple, 10U);
            DeserializeValueYAML(fontImportOptionsNode, "ItalicStyle", fontImportOptions->ItalicStyle, 35U);

            return fontImportOptions;
        }
        else
        {
            CW_ENGINE_WARN("Metadata file does not have valid import options. Correspnding asset may be broken.");
            return CreateRef<ImportOptions>();
        }
        return nullptr;
    }
} // namespace Crowny
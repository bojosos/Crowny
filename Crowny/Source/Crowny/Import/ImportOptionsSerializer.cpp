#include "cwpch.h"

#include "Crowny/Import/ImportOptionsSerializer.h"

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
            out << YAML::BeginMap;
            out << YAML::Key << "AudioImporter" << YAML::Value;
            out << YAML::BeginMap << YAML::Key << "Format" << YAML::Value << (uint32_t)audioImportOptions->Format
                << YAML::EndMap;
            out << YAML::BeginMap << YAML::Key << "ReadMode" << YAML::Value << (uint32_t)audioImportOptions->ReadMode
                << YAML::EndMap;
            out << YAML::BeginMap << YAML::Key << "Is3D" << YAML::Value << audioImportOptions->Is3D << YAML::EndMap;
            out << YAML::BeginMap << YAML::Key << "BitDepth" << YAML::Value << audioImportOptions->BitDepth
                << YAML::EndMap;
            out << YAML::BeginMap << YAML::Key << "Quality" << YAML::Value << audioImportOptions->Quality
                << YAML::EndMap;
            break;
        }
        case ImportOptionsType::Shader: {
            Ref<ShaderImportOptions> shaderImportOptions = std::static_pointer_cast<ShaderImportOptions>(importOptions);
            out << YAML::BeginMap;
            out << YAML::Key << "ShaderImproter" << YAML::Value;
            // out << YAML::Key << shaderImportOptions->GetDefines();
            out << YAML::EndMap;
            break;
        }
        case ImportOptionsType::Texture: {
            Ref<TextureImportOptions> textureImportOptions =
              std::static_pointer_cast<TextureImportOptions>(importOptions);
            out << YAML::BeginMap;
            out << YAML::Key << "TextureImporter" << YAML::Value;
            out << YAML::Key << "AutoFormat" << YAML::Value << textureImportOptions->AutomaticFormat << YAML::EndMap;
            out << YAML::Key << "CpuCached" << YAML::Value << textureImportOptions->CpuCached << YAML::EndMap;
            out << YAML::Key << "Format" << YAML::Value << (uint32_t)textureImportOptions->Format << YAML::EndMap;
            out << YAML::Key << "GenerateMips" << YAML::Value << textureImportOptions->GenerateMips << YAML::EndMap;
            out << YAML::Key << "MaxMip" << YAML::Value << textureImportOptions->MaxMip << YAML::EndMap;
            out << YAML::Key << "Shape" << YAML::Value << (uint32_t)textureImportOptions->Shape << YAML::EndMap;
            out << YAML::Key << "sRGB" << YAML::Value << textureImportOptions->SRGB << YAML::EndMap;
            out << YAML::EndMap;
            break;
        }
        case ImportOptionsType::Script: {
            Ref<CSharpScriptImportOptions> scriptImportOptions =
              std::static_pointer_cast<CSharpScriptImportOptions>(importOptions);
            out << YAML::BeginMap;
            out << YAML::Key << "ScriptImporter";
            out << YAML::EndMap;
            break;
        }
        }
        out << YAML::EndMap;
    }

    Ref<ImportOptions> ImportOptionsSerializer::Deserialize(const YAML::Node& data)
    {
        if (const YAML::Node& audioIo = data["AudioImporter"])
        {
            Ref<AudioClipImportOptions> importOptions = CreateRef<AudioClipImportOptions>();
            if (const auto& format = audioIo["Format"])
            {
                uint32_t formatIdx = format.as<uint32_t>();
                if (formatIdx >= 2)
                    CW_ENGINE_WARN("Audio format \'{0}\' in metadata file is invalid.", formatIdx);
                else
                    importOptions->Format = (AudioFormat)formatIdx;
            }
            if (const auto& readMode = audioIo["ReadMode"])
            {
                uint32_t readModeIdx = readMode.as<uint32_t>();
                if (readModeIdx >= 3)
                    CW_ENGINE_WARN("Audio read mode \'{0}\' in metadata file is invalid.", readModeIdx);
                else
                    importOptions->ReadMode = (AudioReadMode)readModeIdx;
            }
            if (const auto& is3D = audioIo["Is3D"])
            {
                bool is3Dval = is3D.as<bool>();
                importOptions->Is3D = is3Dval;
            }
            if (const auto& bitDepth = audioIo["BitDepth"])
            {
                uint32_t bitDepthVal = bitDepth.as<uint32_t>();
                if (bitDepthVal != 8 && bitDepthVal != 16 && bitDepthVal != 24 && bitDepthVal != 32)
                    CW_ENGINE_WARN("Bit depth \'{0}\' in metadata file is invalid.", bitDepthVal);
                importOptions->BitDepth = bitDepthVal;
            }
            if (const auto& quality = audioIo["Quality"])
            {
                float qualityVal = quality.as<float>();
                if (qualityVal < 0 || qualityVal > 1)
                    CW_ENGINE_WARN("Quality  \'{0}\' in metadata file is invalid.", qualityVal);
                importOptions->Quality = qualityVal;
            }
            return importOptions;
        }
        else if (const YAML::Node& textureIo = data["TextureImporter"])
        {
            bool AutomaticFormat = true;
            TextureFormat Format = TextureFormat::RGBA8;
            TextureShape Shape = TextureShape::TEXTURE_2D;
            bool GenerateMips = false;
            uint32_t MaxMip = 0;
            bool CpuCached = false;
            bool SRGB = false;
            Ref<TextureImportOptions> importOptions = CreateRef<TextureImportOptions>();
            if (const auto& format = textureIo["Format"])
            {
                uint32_t formatIdx = format.as<uint32_t>();
                if (formatIdx >= (uint32_t)TextureFormat::FormatCount)
                    CW_ENGINE_WARN("Texture format \'{0}\' in metadata file is invalid.", formatIdx);
                else
                    importOptions->Format = (TextureFormat)formatIdx;
            }
            if (const auto& autoFormat = audioIo["AutoFormat"])
            {
                bool autoFormatVal = autoFormat.as<bool>();
                importOptions->AutomaticFormat = autoFormatVal;
            }
            if (const auto& generateMips = audioIo["GenerateMips"])
            {
                bool generateMipsVal = generateMips.as<bool>();
                importOptions->GenerateMips = generateMipsVal;
            }
            if (const auto& shape = audioIo["Shape"])
            {
                uint32_t shapeIdx = shape.as<uint32_t>();
                if (shapeIdx >= 4)
                    CW_ENGINE_WARN("Texture shape \'{0}\' in metadata file is invalid.", shapeIdx);
                importOptions->Shape = (TextureShape)shapeIdx;
            }
            if (const auto& cpuCached = audioIo["CpuCached"])
            {
                bool cpuCachedVal = cpuCached.as<bool>();
                importOptions->CpuCached = cpuCachedVal;
            }
            if (const auto& sRgb = audioIo["sRGB"])
            {
                bool sRgbVal = sRgb.as<bool>();
                importOptions->SRGB = sRgbVal;
            }
            if (const auto& maxMip = audioIo["MaxMip"])
            {
                uint32_t maxMipVal = maxMip.as<uint32_t>();
                importOptions->MaxMip = maxMipVal;
            }
            return importOptions;
        }
        else if (const auto& shaderIo = data["ShaderImporter"])
        {
            Ref<ShaderImportOptions> importOptions = CreateRef<ShaderImportOptions>();
            if (const auto& mapNode = shaderIo["Defines"])
            {
                // const auto& map = mapNode.as<UnorderedMap<String, String>>();
                // for (const auto& kvp : map)
                // importOptions->SetDefine(kvp.first, kvp.second);
            }
            return importOptions;
        }
        else
        {
            CW_ENGINE_WARN("Metadata file does not have valid import options. Correspnding asset may be broken.");
            return CreateRef<ImportOptions>();
        }
    }
} // namespace Crowny
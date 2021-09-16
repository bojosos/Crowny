#pragma once

#include "Crowny/Audio/AudioClip.h"

namespace Crowny
{

    class ImportOptions
    {
    public:
        virtual Ref<ImportOptions> Clone() const = 0;
    };

    class TextureImportOptions : public ImportOptions
    {
    public:
        TextureImportOptions() = default;

        TextureFormat Format = TextureFormat::RGBA8;
        TextureShape Shape = TextureShape::TEXTURE_2D;
        bool GenerateMips = false;
        uint32_t MaxMip = 0;
        bool CpuCached = false;
        bool SRGB = false;
        // CubemapSourceType CubemapSource = CubemapSourceType::Faces;

        virtual Ref<ImportOptions> Clone() const override
        {
            Ref<TextureImportOptions> clone = CreateRef<TextureImportOptions>();
            // clone->Format = Format;
            // clone->Shape = Shape;
            // clone->GenerateMips = GenerateMips;
            // clone->MaxMip = MaxMip;
            // clone->CpuCached = CpuCached;
            // clone->SRGB = SRGB;
            *clone = *this;
            return clone;
        }
    };

    class AudioClipImportOptions : public ImportOptions
    {
    public:
        AudioFormat Format = AudioFormat::VORBIS;
        AudioReadMode ReadMode;
        bool Is3D = true;
        uint32_t BitDepth = 16;

        virtual Ref<ImportOptions> Clone() const override
        {
            Ref<AudioClipImportOptions> clone = CreateRef<AudioClipImportOptions>();
            // clone->Format = Format;
            // clone->ReadMode = ReadMode;
            // clone->Is3D = Is3D;
            // clone->BitDepth = BitDepth;
            *clone = *this;
            return clone;
        }
    };

    enum class ShaderLanguage
    {
        VKSL,
        GLSL,
        HLSL,
        MSL
    };

    class ShaderImportOptions : public ImportOptions
    {
    public:
        ShaderLanguage Language;

        void SetDefine(const std::string& key, const std::string& value) { m_Defines[key] = value; }

        bool GetDefine(const std::string& key, std::string& value) const
        {
            auto iter = m_Defines.find(key);
            if (iter != m_Defines.end())
            {
                value = iter->second;
                return true;
            }
            return false;
        }

        bool HasDefine(const std::string& key) const
        {
            auto iter = m_Defines.find(key);
            if (iter != m_Defines.end())
                return true;
            return false;
        }

        void RemoveDefine(const std::string& key) { m_Defines.erase(key); }

        virtual Ref<ImportOptions> Clone() const override
        {
            Ref<ShaderImportOptions> clone = CreateRef<ShaderImportOptions>();
            *clone = *this;
            return clone;
        }

    private:
        std::unordered_map<std::string, std::string> m_Defines;
    };

    class CSharpScriptImportOptions : public ImportOptions
    {
    public:
        virtual Ref<ImportOptions> Clone() const override
        {
            Ref<CSharpScriptImportOptions> clone = CreateRef<CSharpScriptImportOptions>();
            *clone = *this;
            return clone;
        }
    };

} // namespace Crowny
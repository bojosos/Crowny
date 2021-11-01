#pragma once

#include "Crowny/Assets/CerealDataStreamArchive.h"
#include "Crowny/Audio/AudioClip.h"

#include "Crowny/Utils/ShaderCompiler.h"

namespace Crowny
{

    enum class ImportOptionsType
    {
        None,
        Texture,
        Shader,
        AudioClip,
        Script
    };

    class ImportOptions
    {
    public:
        ImportOptions() = default;
        virtual ImportOptionsType GetImportOptionsType() const { return ImportOptionsType::None; }
        virtual Ref<ImportOptions> Clone() const { return CreateRef<ImportOptions>(); };
    };

    class TextureImportOptions : public ImportOptions
    {
    public:
        TextureImportOptions() = default;

        bool AutomaticFormat = true;
        TextureFormat Format = TextureFormat::RGBA8;
        TextureShape Shape = TextureShape::TEXTURE_2D;
        bool GenerateMips = false;
        uint32_t MaxMip = 0;
        bool CpuCached = false;
        bool SRGB = false;
        // CubemapSourceType CubemapSource = CubemapSourceType::Faces;

        virtual ImportOptionsType GetImportOptionsType() const override { return ImportOptionsType::Texture; }

        virtual Ref<ImportOptions> Clone() const override
        {
            Ref<TextureImportOptions> clone = CreateRef<TextureImportOptions>();
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

        // Only available for Ogg Vorbis
        float Quality = 1.0f;

        virtual ImportOptionsType GetImportOptionsType() const override { return ImportOptionsType::AudioClip; }

        virtual Ref<ImportOptions> Clone() const override
        {
            Ref<AudioClipImportOptions> clone = CreateRef<AudioClipImportOptions>();
            *clone = *this;
            return clone;
        }
    };

    class ShaderImportOptions : public ImportOptions
    {
    public:
        ShaderLanguage Language;

        void SetDefine(const String& key, const String& value) { m_Defines[key] = value; }

        bool GetDefine(const String& key, String& value) const
        {
            auto iter = m_Defines.find(key);
            if (iter != m_Defines.end())
            {
                value = iter->second;
                return true;
            }
            return false;
        }

        bool HasDefine(const String& key) const
        {
            auto iter = m_Defines.find(key);
            if (iter != m_Defines.end())
                return true;
            return false;
        }

        void RemoveDefine(const String& key) { m_Defines.erase(key); }

        const UnorderedMap<String, String>& GetDefines() const { return m_Defines; }

        virtual ImportOptionsType GetImportOptionsType() const override { return ImportOptionsType::Shader; }

        virtual Ref<ImportOptions> Clone() const override
        {
            Ref<ShaderImportOptions> clone = CreateRef<ShaderImportOptions>();
            *clone = *this;
            return clone;
        }

        friend void Serialize(BinaryDataStreamOutputArchive& archive, ShaderImportOptions& importOptions);

    private:
        UnorderedMap<String, String> m_Defines;
    };

    class CSharpScriptImportOptions : public ImportOptions
    {
    public:
        virtual ImportOptionsType GetImportOptionsType() const override { return ImportOptionsType::Script; }

        virtual Ref<ImportOptions> Clone() const override
        {
            Ref<CSharpScriptImportOptions> clone = CreateRef<CSharpScriptImportOptions>();
            *clone = *this;
            return clone;
        }
    };

    template <typename Archive> void Serialize(Archive& archive, TextureImportOptions& importOptions)
    {
        archive(importOptions.AutomaticFormat, importOptions.CpuCached, importOptions.Format,
                importOptions.GenerateMips, importOptions.MaxMip, importOptions.Shape, importOptions.SRGB);
    }

    template <typename Archive> void Serialize(Archive& archive, AudioClipImportOptions& importOptions)
    {
        archive(importOptions.Format, importOptions.Quality, importOptions.ReadMode, importOptions.BitDepth,
                importOptions.Is3D);
    }

    template <typename Archive> void Serialize(Archive& archive, ShaderImportOptions& importOptions)
    {
        archive(importOptions.Language, importOptions.m_Defines);
    }

} // namespace Crowny

#pragma once

#include "Crowny/Audio/AudioClip.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Serialization/CerealDataStreamArchive.h"
#include "Crowny/Utils/ShaderCompiler.h"

namespace Crowny
{

    enum class ImportOptionsType
    {
        None,
        Texture,
        Shader,
        AudioClip,
        Script,
        Font,
        Mesh
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
        AudioReadMode ReadMode = AudioReadMode::LoadCompressed;
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
        ShaderLanguage Language = ShaderLanguage::VKSL;

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
        UnorderedMap<String, String>& GetDefines() { return m_Defines; }

        virtual ImportOptionsType GetImportOptionsType() const override { return ImportOptionsType::Shader; }

        virtual Ref<ImportOptions> Clone() const override
        {
            Ref<ShaderImportOptions> clone = CreateRef<ShaderImportOptions>();
            *clone = *this;
            return clone;
        }

    private:
        CW_SERIALIZABLE(ShaderImportOptions);
        UnorderedMap<String, String> m_Defines;
    };

    class CSharpScriptImportOptions : public ImportOptions
    {
    public:
        bool IsEditorScript = false;
        virtual ImportOptionsType GetImportOptionsType() const override { return ImportOptionsType::Script; }

        virtual Ref<ImportOptions> Clone() const override
        {
            Ref<CSharpScriptImportOptions> clone = CreateRef<CSharpScriptImportOptions>();
            *clone = *this;
            return clone;
        }
    };

    class FontImportOptions : public ImportOptions
    {
    public:
        // TODO: Somehow integrate the ImageType stuff from msdfgen.
        // The first two modes are rasterized fonts I think, the others are fancy sdf stuff.
        bool GetKerningData = true;
        bool AutomaticFontSampling = true;
        uint32_t SampingFontSize = 64;
        bool AutoSizeAtlas = false;
        Font::AtlasDimensionsConstraint AtlasDimensionsConstraint =
          Font::AtlasDimensionsConstraint::POWER_OF_TWO_SQUARE;
        uint32_t AtlasWidth = 1024;
        uint32_t AtlasHeight = 1024;
        CharsetRange Range = CharsetRange::ASCII;
        String CustomCharset;
        uint32_t Padding = 0;
        bool DynamicFontAtlas = false;
        float BoldWeight = 0.75f;
        float BoldSpacing = 7.0f;
        uint32_t TabMultiple = 10;
        uint32_t ItalicStyle = 35;

        virtual ImportOptionsType GetImportOptionsType() const override { return ImportOptionsType::Font; }

        virtual Ref<ImportOptions> Clone() const override
        {
            Ref<FontImportOptions> clone = CreateRef<FontImportOptions>();
            *clone = *this;
            return clone;
        }

    private:
        CW_SERIALIZABLE(FontImportOptions);
    };

    enum class NormalsImportMode
    {
        Import,
        Calculate,
        None,
        Count
    };

    enum class MeshIndexFormat
    {
        Auto,
        Index16,
        Index32,
        Count
    };

    class MeshImportOptions : public ImportOptions
    {
    public:
        float ScaleFactor = 1.0f;
        bool CpuCached = false;
        bool Compress = false;
        bool Optimize = false;
        bool KeepQuads = false;
        bool SmoothNormals = false;
        float SmoothingAngle = 175.0f;
        NormalsImportMode NormalsMode = NormalsImportMode::Import;
        NormalsImportMode TangentsMode = NormalsImportMode::Import;
        MeshIndexFormat IndexFormat = MeshIndexFormat::Auto;

        virtual ImportOptionsType GetImportOptionsType() const override { return ImportOptionsType::Mesh; }

        virtual Ref<ImportOptions> Clone() const override
        {
            Ref<MeshImportOptions> clone = CreateRef<MeshImportOptions>();
            *clone = *this;
            return clone;
        }

    private:
        CW_SERIALIZABLE(MeshImportOptions)
    };

} // namespace Crowny

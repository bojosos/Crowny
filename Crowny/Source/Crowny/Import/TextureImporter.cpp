#include "cwpch.h"

#include "Crowny/Import/TextureImporter.h"
#include "Crowny/Renderer/EnvironmentMap.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Import/ImageLoader.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/BasisTextureCodec.h"
#include "Crowny/Serialization/ImportOptionsSerializer.h"
#include "Crowny/Utils/Cryptography.h"
#include <fstream>
#include <yaml-cpp/yaml.h>

namespace Crowny
{
    namespace
    {
        Path CookDirectory(Path directory)
        {
            if (!directory.empty())
                return directory;
            const auto* application = Application::TryGet();
            if (!application || application->GetInternalDirectory().empty())
                return {};
            const Path& internal = application->GetInternalDirectory();
            return (internal.filename() == "Assets" ? internal.parent_path() : internal) / "ImportCache/Textures-v1";
        }

        String CookSettings(const Ref<const TextureImportOptions>& options)
        {
            YAML::Emitter settings;
            settings << YAML::BeginMap;
            ImportOptionsSerializer::Serialize(settings, options->Clone());
            settings << YAML::EndMap;
            return settings.c_str();
        }

        Path SourceCookPath(StringView source, const Ref<const TextureImportOptions>& options, const Path& directory)
        {
            const Path root = CookDirectory(directory);
            if (root.empty() || options->DiskFormat == TextureDiskFormat::None)
                return {};
            const String key = "source-cook-v2|" + Cryptography::SHA256(String(source)) + "|" + CookSettings(options);
            return root / "Sources-v1" / (Cryptography::SHA256(key) + ".sourcecook");
        }

        Ref<Texture> ReadSourceCook(const Path& path, StringView name, const Ref<const TextureImportOptions>& options)
        {
            if (path.empty())
                return nullptr;
            std::ifstream stream(path, std::ios::binary | std::ios::ate);
            const auto size = stream.tellg();
            if (size <= 68 || size > 512 * 1024 * 1024)
                return nullptr;
            String contents(static_cast<size_t>(size), '\0');
            stream.seekg(0);
            if (!stream.read(contents.data(), size) || Cryptography::SHA256(contents.substr(64)) != contents.substr(0, 64))
                return nullptr;
            uint32_t format;
            std::memcpy(&format, contents.data() + 64, sizeof(format));
            const auto sourceFormat = static_cast<TextureFormat>(format);
            BasisTextureInfo info;
            if (!PixelUtils::IsValidFormat(sourceFormat) || PixelUtils::IsCompressedFormat(sourceFormat) || PixelUtils::IsDepthFormat(sourceFormat) ||
                PixelUtils::IsFloatFormat(sourceFormat) || !BasisTextureCodec::Inspect(contents.data() + 68, contents.size() - 68, info) ||
                info.Layers != 1 || info.Faces != 1 || info.Levels == 0 || info.DiskFormat != options->DiskFormat || info.SRGB != options->SRGB)
                return nullptr;
            TextureDesc desc;
            desc.Width = info.Width;
            desc.Height = info.Height;
            desc.MipLevels = info.Levels - 1;
            desc.GenerateMipmaps = false;
            desc.Format = sourceFormat;
            desc.sRGB = info.SRGB;
            desc.DebugName = name;
            const auto texture = Texture::CreateDeferred(desc);
            texture->SetEncodedSourceData(info.DiskFormat, sourceFormat, Vector<uint8_t>(contents.begin() + 68, contents.end()));
            texture->SetCpuCached(options->CpuCached);
            texture->SetName(String(name));
            return texture;
        }

        void WriteSourceCook(const Path& path, const Ref<Texture>& texture)
        {
            if (path.empty() || !texture || texture->GetEncodedSourceData().empty())
                return;
            // Only raster Basis results use this shortcut. KTX containers retain their original import rules.
            BasisTextureInfo info;
            const auto& encoded = texture->GetEncodedSourceData();
            if (!BasisTextureCodec::Inspect(encoded.data(), encoded.size(), info) || info.Layers != 1 || info.Faces != 1)
                return;
            const uint32_t format = static_cast<uint32_t>(texture->GetDesc().Format);
            String payload(reinterpret_cast<const char*>(&format), sizeof(format));
            payload.append(reinterpret_cast<const char*>(encoded.data()), encoded.size());
            const String contents = Cryptography::SHA256(payload) + payload;
            FileSystem::WriteFileAtomic(path, reinterpret_cast<const byte*>(contents.data()), contents.size());
        }

        Path CookCachePath(const PixelData& source, const Ref<const TextureImportOptions>& options, Path directory)
        {
            directory = CookDirectory(directory);
            if (directory.empty())
                return {};
            // Bump the directory/version when changing mip generation or encoder policy.
            // Hash decoded pixels, not file timestamps, so edits and embedded textures
            // invalidate correctly while equivalent sources can share a cook.
            String pixels(reinterpret_cast<const char*>(source.GetData()), source.GetSlicePitch() * source.GetDepth());
            const String key = "texture-cook-v2|" + std::to_string(source.GetWidth()) + "|" + std::to_string(source.GetHeight()) + "|" +
                               std::to_string(static_cast<uint32_t>(source.GetFormat())) + "|" + Cryptography::SHA256(pixels) + "|" +
                               CookSettings(options);
            return directory / (Cryptography::SHA256(key) + ".texturecook");
        }

        bool ReadCookCache(const Path& path, Vector<uint8_t>& encoded, BasisTextureInfo& info)
        {
            if (path.empty())
                return false;
            std::ifstream stream(path, std::ios::binary | std::ios::ate);
            const auto size = stream.tellg();
            // A cache is optional and disposable. Reject damaged entries and recook.
            if (size <= 64 || size > 512 * 1024 * 1024)
                return false;
            String contents(static_cast<size_t>(size), '\0');
            stream.seekg(0);
            if (!stream.read(contents.data(), size))
                return false;
            const String payload = contents.substr(64);
            if (Cryptography::SHA256(payload) != contents.substr(0, 64) || !BasisTextureCodec::Inspect(payload.data(), payload.size(), info))
                return false;
            encoded.assign(payload.begin(), payload.end());
            return true;
        }

        void WriteCookCache(const Path& path, const Vector<uint8_t>& encoded)
        {
            if (path.empty())
                return;
            const String payload(reinterpret_cast<const char*>(encoded.data()), encoded.size());
            const String contents = Cryptography::SHA256(payload) + payload;
            FileSystem::WriteFileAtomic(path, reinterpret_cast<const byte*>(contents.data()), contents.size());
        }

        uint32_t ResolveLevelCount(uint32_t width, uint32_t height, bool generateMips, uint32_t maxMip, uint32_t availableLevels = 0)
        {
            uint32_t levels = generateMips ? PixelUtils::GetMaxMipCount(width, height) : 1u;
            if (availableLevels != 0)
                levels = std::min(levels, availableLevels);
            if (maxMip != 0)
                levels = std::min(levels, maxMip + 1u);
            return std::max(levels, 1u);
        }

        bool IsHighPrecisionFormat(TextureFormat format)
        {
            if (PixelUtils::IsFloatFormat(format))
                return true;
            int bitDepths[4] = {};
            PixelUtils::GetBitDepths(format, bitDepths);
            return *std::max_element(std::begin(bitDepths), std::end(bitDepths)) > 8;
        }

        Ref<PixelData> FlipPixels(const PixelData& source)
        {
            if (!source.IsValid() || PixelUtils::IsCompressedFormat(source.GetFormat()))
                return nullptr;
            Ref<PixelData> output = PixelData::Create(source.GetWidth(), source.GetHeight(), source.GetDepth(), source.GetFormat());
            if (!output || !output->IsValid() || source.GetRowPitch() != output->GetRowPitch())
                return nullptr;
            for (uint32_t z = 0; z < source.GetDepth(); z++)
            {
                for (uint32_t y = 0; y < source.GetHeight(); y++)
                {
                    const uint8_t* sourceRow = source.GetData() + static_cast<size_t>(z) * source.GetSlicePitch() +
                                               static_cast<size_t>(source.GetHeight() - y - 1u) * source.GetRowPitch();
                    uint8_t* outputRow =
                      output->GetData() + static_cast<size_t>(z) * output->GetSlicePitch() + static_cast<size_t>(y) * output->GetRowPitch();
                    std::memcpy(outputRow, sourceRow, source.GetRowPitch());
                }
            }
            return output;
        }

        TextureFormat GetColorNormalizedFormat(TextureFormat sourceFormat, bool alpha)
        {
            switch (sourceFormat)
            {
            case TextureFormat::R8:
            case TextureFormat::RG8:
                return alpha ? TextureFormat::RGBA8 : TextureFormat::RGB8;
            case TextureFormat::R16:
            case TextureFormat::RG16:
                return TextureFormat::RGBA16;
            case TextureFormat::R32F:
            case TextureFormat::RG32F:
                return alpha ? TextureFormat::RGBA32F : TextureFormat::RGB32F;
            default:
                return TextureFormat::NONE;
            }
        }

        Ref<PixelData> NormalizeColorChannels(const PixelData& source, ImageChannelLayout layout)
        {
            if (layout != ImageChannelLayout::Gray && layout != ImageChannelLayout::GrayAlpha)
                return CreateRef<PixelData>(source);

            const bool hasAlpha = layout == ImageChannelLayout::GrayAlpha;
            const TextureFormat targetFormat = GetColorNormalizedFormat(source.GetFormat(), hasAlpha);
            if (!PixelUtils::IsValidFormat(targetFormat))
                return nullptr;
            Ref<PixelData> output = PixelData::Create(source.GetWidth(), source.GetHeight(), source.GetDepth(), targetFormat);
            if (!output || !output->IsValid())
                return nullptr;
            for (uint32_t z = 0; z < source.GetDepth(); z++)
            {
                for (uint32_t y = 0; y < source.GetHeight(); y++)
                {
                    for (uint32_t x = 0; x < source.GetWidth(); x++)
                    {
                        const glm::vec4 sourceColor = source.GetColorAt(x, y, z);
                        const glm::vec4 color(sourceColor.r, sourceColor.r, sourceColor.r, hasAlpha ? sourceColor.g : 1.0f);
                        output->SetColorAt(x, y, z, color);
                    }
                }
            }
            return output;
        }

        bool ConvertSRGBToLinear(PixelData& pixels)
        {
            for (uint32_t z = 0; z < pixels.GetDepth(); z++)
            {
                for (uint32_t y = 0; y < pixels.GetHeight(); y++)
                {
                    for (uint32_t x = 0; x < pixels.GetWidth(); x++)
                    {
                        glm::vec4 color;
                        if (!pixels.TryGetColorAt(x, y, z, color))
                            return false;
                        color.r = SRGBToLinear(color.r);
                        color.g = SRGBToLinear(color.g);
                        color.b = SRGBToLinear(color.b);
                        if (!pixels.TrySetColorAt(x, y, z, color))
                            return false;
                    }
                }
            }
            return true;
        }

        ImageChannelLayout GetPixelLayout(TextureFormat format)
        {
            switch (PixelUtils::GetComponentCount(format))
            {
            case 1:
                return ImageChannelLayout::Gray;
            case 2:
                return ImageChannelLayout::RG;
            case 3:
                return ImageChannelLayout::RGB;
            case 4:
                return ImageChannelLayout::RGBA;
            default:
                return ImageChannelLayout::Unknown;
            }
        }
    } // namespace

    bool TextureImporter::IsExtensionSupportedStatic(const String& ext) { return ImageLoader::SupportsExtension(ext); }

    bool TextureImporter::IsExtensionSupported(const String& ext) const { return IsExtensionSupportedStatic(ext); }

    bool TextureImporter::IsMagicNumSupported(uint8_t* num, uint32_t numSize) const { return ImageLoader::SupportsSignature(num, numSize); }

    Ref<Asset> TextureImporter::Import(const Path& filepath, Ref<const ImportOptions> importOptions)
    {
        const Ref<const TextureImportOptions> options = StaticRefCast<const TextureImportOptions>(importOptions);
        if (options == nullptr)
            return nullptr;

        ImageLoadOptions loadOptions;
        loadOptions.DecodeTextureContainers = false;
        loadOptions.FlipVertically = true;
        loadOptions.Preserve16Bit = true;
        String source;
        if (!CookDirectory(m_CacheDirectory).empty() && options->DiskFormat != TextureDiskFormat::None)
        {
            std::ifstream stream(filepath, std::ios::binary | std::ios::ate);
            const auto size = stream.tellg();
            if (size > 0 && size < 512 * 1024 * 1024)
            {
                source.resize(static_cast<size_t>(size));
                stream.seekg(0);
                if (!stream.read(source.data(), size))
                    source.clear();
            }
        }
        const Path sourceCache = source.empty() ? Path{} : SourceCookPath(source, options, m_CacheDirectory);
        if (const auto cached = ReadSourceCook(sourceCache, filepath.filename().string(), options))
            return cached;
        ImageLoadResult image =
          source.empty()
            ? ImageLoader::Load(ImageLoadRequest::FromFile(filepath, loadOptions))
            : ImageLoader::Load(ImageLoadRequest::FromMemory(reinterpret_cast<const uint8_t*>(source.data()), source.size(), loadOptions));
        if (!image)
        {
            CW_ENGINE_ERROR("Could not import texture '{}': {}", filepath, image.Error);
            return nullptr;
        }
        const bool raster = image.Info.Container == ImageContainerFormat::Raster;
        const auto texture = ImportLoadedImage(std::move(image), filepath.filename().string(), options, m_CacheDirectory);
        if (raster)
            WriteSourceCook(sourceCache, texture);
        return texture;
    }

    Ref<Texture> TextureImporter::ImportFromMemory(const uint8_t* data, size_t size, StringView name, Ref<const TextureImportOptions> importOptions)
    {
        if (importOptions == nullptr || data == nullptr || size == 0)
            return nullptr;
        ImageLoadOptions loadOptions;
        loadOptions.DecodeTextureContainers = false;
        loadOptions.FlipVertically = true;
        loadOptions.Preserve16Bit = true;
        const Path sourceCache = SourceCookPath(StringView(reinterpret_cast<const char*>(data), size), importOptions, {});
        if (const auto cached = ReadSourceCook(sourceCache, name, importOptions))
            return cached;
        ImageLoadResult image = ImageLoader::Load(ImageLoadRequest::FromMemory(data, size, loadOptions));
        if (!image)
        {
            CW_ENGINE_ERROR("Could not import embedded texture '{}': {}", name, image.Error);
            return nullptr;
        }
        const bool raster = image.Info.Container == ImageContainerFormat::Raster;
        const auto texture = ImportLoadedImage(std::move(image), name, importOptions);
        if (raster)
            WriteSourceCook(sourceCache, texture);
        return texture;
    }

    Ref<Texture> TextureImporter::ImportFromPixels(const Ref<PixelData>& pixels, StringView name, Ref<const TextureImportOptions> importOptions,
                                                   bool flipVertically)
    {
        if (importOptions == nullptr || !pixels || !pixels->IsValid() || PixelUtils::IsCompressedFormat(pixels->GetFormat()))
            return nullptr;

        ImageLoadResult image;
        image.Status = ImageLoadStatus::Succeeded;
        image.Info.Container = ImageContainerFormat::Raster;
        image.Info.FileFormat = ImageFileFormat::OtherRaster;
        image.Info.Width = pixels->GetWidth();
        image.Info.Height = pixels->GetHeight();
        image.Info.Depth = pixels->GetDepth();
        image.Info.Channels = PixelUtils::GetComponentCount(pixels->GetFormat());
        image.Info.ChannelLayout = GetPixelLayout(pixels->GetFormat());
        image.Info.PixelFormat = pixels->GetFormat();
        image.Info.HasAlpha = PixelUtils::HasAlpha(pixels->GetFormat());
        image.Info.IsFloat = PixelUtils::IsFloatFormat(pixels->GetFormat());
        int bitDepths[4] = {};
        PixelUtils::GetBitDepths(pixels->GetFormat(), bitDepths);
        image.Info.BitDepth = static_cast<uint32_t>(*std::max_element(std::begin(bitDepths), std::end(bitDepths)));
        image.Pixels = flipVertically ? FlipPixels(*pixels) : CreateRef<PixelData>(*pixels);
        if (!image.Pixels)
            return nullptr;
        image.Subresources.push_back({ 0, 0, 0, image.Pixels });
        return ImportLoadedImage(std::move(image), name, importOptions);
    }

    Ref<Texture> TextureImporter::ImportLoadedImage(ImageLoadResult image, StringView name, const Ref<const TextureImportOptions>& options,
                                                    const Path& cacheDirectory)
    {
        const String debugName(name);

        if (image.Info.Container == ImageContainerFormat::KTX2)
        {
            if (image.SourceData.empty())
            {
                CW_ENGINE_ERROR("Could not import texture '{}': KTX2 source data is missing", debugName);
                return nullptr;
            }
            TextureDesc desc;
            desc.Width = image.Info.Width;
            desc.Height = image.Info.Height;
            desc.Depth = 1;
            const uint64_t sliceCount = static_cast<uint64_t>(image.Info.Layers) * image.Info.Faces;
            if (sliceCount == 0 || sliceCount > std::numeric_limits<uint32_t>::max())
            {
                CW_ENGINE_ERROR("Could not import texture '{}': KTX2 layer and face count is out of range", debugName);
                return nullptr;
            }
            desc.Faces = static_cast<uint32_t>(sliceCount);
            desc.Shape = image.Info.GetRuntimeShape();
            const uint64_t requestedLevels = options->MaxMip == 0 ? image.Info.MipLevels : static_cast<uint64_t>(options->MaxMip) + 1u;
            const uint32_t importedLevels = static_cast<uint32_t>(std::min<uint64_t>(image.Info.MipLevels, requestedLevels));
            desc.MipLevels = importedLevels - 1u;
            desc.Format = image.Info.PixelFormat;
            desc.sRGB = image.Info.SRGB;
            desc.DebugName = debugName;
            Ref<Texture> texture = Texture::CreateDeferred(desc);
            texture->SetEncodedSourceData(image.Info.DiskFormat, desc.Format, std::move(image.SourceData));
            texture->SetCpuCached(options->CpuCached);
            texture->SetName(debugName);
            return texture;
        }

        Ref<PixelData> source = image.Pixels;
        if (!source)
        {
            CW_ENGINE_ERROR("Could not import texture '{}': decoded pixels are missing", debugName);
            return nullptr;
        }
        if (options->Shape != TextureShape::TEXTURE_2D)
            CW_ENGINE_WARN("Texture '{}' requested an unsupported raster shape; importing it as 2D", debugName);

        if (options->MipMode == TextureMipMode::Color &&
            (image.Info.ChannelLayout == ImageChannelLayout::Gray || image.Info.ChannelLayout == ImageChannelLayout::GrayAlpha))
        {
            source = NormalizeColorChannels(*source, image.Info.ChannelLayout);
            if (!source)
            {
                CW_ENGINE_ERROR("Could not normalize color channels for texture '{}'", debugName);
                return nullptr;
            }
        }

        TextureFormat sourceFormat = source->GetFormat();
        if (!options->AutomaticFormat)
        {
            if (!PixelUtils::IsValidFormat(options->Format) || PixelUtils::IsCompressedFormat(options->Format) ||
                PixelUtils::IsDepthFormat(options->Format) ||
                (PixelUtils::IsIntegerFormat(options->Format) && !PixelUtils::IsNormalizedFormat(options->Format)))
            {
                CW_ENGINE_ERROR("Texture '{}' requested unsupported import format {}", debugName, PixelUtils::GetFormatName(options->Format));
                return nullptr;
            }
            Ref<PixelData> converted = PixelData::Create(source->GetWidth(), source->GetHeight(), source->GetDepth(), options->Format);
            if (!PixelUtils::ConvertPixels(*source, *converted))
                return nullptr;
            source = converted;
            sourceFormat = options->Format;
        }
        else if (sourceFormat == TextureFormat::RGB16)
        {
            Ref<PixelData> converted = PixelData::Create(source->GetWidth(), source->GetHeight(), source->GetDepth(), TextureFormat::RGBA16);
            if (!converted || !PixelUtils::ConvertPixels(*source, *converted))
                return nullptr;
            source = std::move(converted);
            sourceFormat = TextureFormat::RGBA16;
        }

        const bool highPrecision = IsHighPrecisionFormat(sourceFormat);
        const bool decodeSRGBOnCpu = highPrecision && !image.Info.IsHDR && options->SRGB && options->MipMode == TextureMipMode::Color;
        if (decodeSRGBOnCpu && !ConvertSRGBToLinear(*source))
        {
            CW_ENGINE_ERROR("Could not linearize high-precision color texture '{}'", debugName);
            return nullptr;
        }
        const bool useBasis = !highPrecision && options->DiskFormat != TextureDiskFormat::None;
        if (highPrecision && options->DiskFormat != TextureDiskFormat::None)
            CW_ENGINE_WARN("High-precision texture '{}' cannot use the LDR Basis modes; storing {} data", debugName,
                           PixelUtils::GetFormatName(sourceFormat));
        TextureDesc desc;
        desc.Width = source->GetWidth();
        desc.Height = source->GetHeight();
        desc.Depth = 1;
        desc.Faces = 1;
        desc.Shape = TextureShape::TEXTURE_2D;
        desc.Format = sourceFormat;
        desc.sRGB = !highPrecision && options->SRGB;
        desc.DebugName = debugName;

        const Path cachePath = useBasis ? CookCachePath(*source, options, cacheDirectory) : Path{};
        Vector<uint8_t> cachedEncoded;
        BasisTextureInfo cachedInfo;
        if (useBasis && ReadCookCache(cachePath, cachedEncoded, cachedInfo) && cachedInfo.Width == desc.Width && cachedInfo.Height == desc.Height &&
            cachedInfo.DiskFormat == options->DiskFormat && cachedInfo.SRGB == desc.sRGB)
        {
            desc.MipLevels = cachedInfo.Levels - 1u;
            desc.GenerateMipmaps = false;
            const Ref<Texture> texture = Texture::CreateDeferred(desc);
            texture->SetEncodedSourceData(options->DiskFormat, sourceFormat, std::move(cachedEncoded));
            texture->SetCpuCached(options->CpuCached);
            texture->SetName(debugName);
            return texture;
        }

        TextureMipGenerationOptions mipOptions;
        mipOptions.Filter = options->MipFilter;
        mipOptions.Mode = options->MipMode;
        mipOptions.MaxLevels = ResolveLevelCount(desc.Width, desc.Height, options->GenerateMips, options->MaxMip);
        mipOptions.SRGB = desc.sRGB;
        mipOptions.Wrap = options->MipWrap;
        mipOptions.PreserveAlphaCoverage = options->PreserveAlphaCoverage;
        mipOptions.AlphaCutoff = options->AlphaCutoff;

        Vector<Ref<PixelData>> mipChain;
        String mipError;
        if (!PixelUtils::GenerateMipChain(*source, mipOptions, mipChain, &mipError))
        {
            CW_ENGINE_ERROR("Could not generate texture mips for '{}': {}", debugName, mipError);
            return nullptr;
        }

        Ref<Texture> texture;
        if (useBasis)
        {
            BasisTextureInfo info;
            Vector<uint8_t> encoded;
            String error;
            BasisTextureSource textureSource;
            textureSource.Levels = static_cast<uint32_t>(mipChain.size());
            textureSource.Subresources = mipChain;
            textureSource.UASTCEffort = options->UASTCEffort;
            if (!BasisTextureCodec::Encode(textureSource, options->DiskFormat, desc.sRGB, encoded, &info, &error))
            {
                CW_ENGINE_ERROR("Could not Basis-compress texture '{}': {}", debugName, error);
                return nullptr;
            }
            desc.MipLevels = std::min(static_cast<uint32_t>(mipChain.size()), info.Levels) - 1u;
            desc.GenerateMipmaps = false;
            WriteCookCache(cachePath, encoded);
            texture = Texture::CreateDeferred(desc);
            texture->SetEncodedSourceData(options->DiskFormat, sourceFormat, std::move(encoded));
        }
        else
        {
            const uint32_t levelCount = static_cast<uint32_t>(mipChain.size());
            desc.MipLevels = levelCount - 1u;
            desc.GenerateMipmaps = false;
            Vector<TextureSubresourceData> subresources;
            subresources.reserve(levelCount);
            for (uint32_t mip = 0; mip < levelCount; mip++)
                subresources.push_back({ mip, 0, std::move(mipChain[mip]) });
            texture = Texture::CreateDeferred(desc);
            texture->SetPendingSubresources(std::move(subresources));
        }

        texture->SetCpuCached(options->CpuCached);
        texture->SetName(debugName);
        return texture;
    }

    Vector<Ref<Asset>> TextureImporter::ImportAll(const Path& path, Ref<const ImportOptions> importOptions)
    {
        const Ref<Asset> texture = Import(path, importOptions);
        if (!texture)
            return {};
        Vector<Ref<Asset>> assets{ texture };
        const auto options = StaticRefCast<const TextureImportOptions>(importOptions);
        if (options->GenerateEnvironmentMap)
        {
            const Ref<EnvironmentMap> environment = EnvironmentMap::CreateDeferred(path);
            environment->SetName(path.stem().string() + " Environment");
            assets.push_back(environment);
        }
        return assets;
    }

    Ref<ImportOptions> TextureImporter::CreateImportOptions() const { return CreateRef<TextureImportOptions>(); }
} // namespace Crowny

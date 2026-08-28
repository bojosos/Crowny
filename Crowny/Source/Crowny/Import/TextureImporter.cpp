#include "cwpch.h"

#include "Crowny/Import/TextureImporter.h"

#include "Crowny/Import/ImageLoader.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/BasisTextureCodec.h"

namespace Crowny
{
    namespace
    {
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
                    uint8_t* outputRow = output->GetData() + static_cast<size_t>(z) * output->GetSlicePitch() +
                                         static_cast<size_t>(y) * output->GetRowPitch();
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
            case TextureFormat::RG8: return alpha ? TextureFormat::RGBA8 : TextureFormat::RGB8;
            case TextureFormat::R16:
            case TextureFormat::RG16: return TextureFormat::RGBA16;
            case TextureFormat::R32F:
            case TextureFormat::RG32F: return alpha ? TextureFormat::RGBA32F : TextureFormat::RGB32F;
            default: return TextureFormat::NONE;
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
            case 1: return ImageChannelLayout::Gray;
            case 2: return ImageChannelLayout::RG;
            case 3: return ImageChannelLayout::RGB;
            case 4: return ImageChannelLayout::RGBA;
            default: return ImageChannelLayout::Unknown;
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
        ImageLoadResult image = ImageLoader::Load(ImageLoadRequest::FromFile(filepath, loadOptions));
        if (!image)
        {
            CW_ENGINE_ERROR("Could not import texture '{}': {}", filepath, image.Error);
            return nullptr;
        }
        return ImportLoadedImage(std::move(image), filepath.filename().string(), options);
    }

    Ref<Texture> TextureImporter::ImportFromMemory(const uint8_t* data, size_t size, StringView name,
                                                   Ref<const TextureImportOptions> importOptions)
    {
        if (importOptions == nullptr || data == nullptr || size == 0)
            return nullptr;
        ImageLoadOptions loadOptions;
        loadOptions.DecodeTextureContainers = false;
        loadOptions.FlipVertically = true;
        loadOptions.Preserve16Bit = true;
        ImageLoadResult image = ImageLoader::Load(ImageLoadRequest::FromMemory(data, size, loadOptions));
        if (!image)
        {
            CW_ENGINE_ERROR("Could not import embedded texture '{}': {}", name, image.Error);
            return nullptr;
        }
        return ImportLoadedImage(std::move(image), name, importOptions);
    }

    Ref<Texture> TextureImporter::ImportFromPixels(const Ref<PixelData>& pixels, StringView name,
                                                   Ref<const TextureImportOptions> importOptions, bool flipVertically)
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

    Ref<Texture> TextureImporter::ImportLoadedImage(ImageLoadResult image, StringView name,
                                                    const Ref<const TextureImportOptions>& options)
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
            const uint64_t requestedLevels = options->MaxMip == 0
                                               ? image.Info.MipLevels
                                               : static_cast<uint64_t>(options->MaxMip) + 1u;
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
                CW_ENGINE_ERROR("Texture '{}' requested unsupported import format {}", debugName,
                                PixelUtils::GetFormatName(options->Format));
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
            if (!BasisTextureCodec::Encode(mipChain, options->DiskFormat, desc.sRGB, encoded, &info, &error))
            {
                CW_ENGINE_ERROR("Could not Basis-compress texture '{}': {}", debugName, error);
                return nullptr;
            }
            desc.MipLevels = std::min(static_cast<uint32_t>(mipChain.size()), info.Levels) - 1u;
            desc.GenerateMipmaps = false;
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

    Ref<ImportOptions> TextureImporter::CreateImportOptions() const { return CreateRef<TextureImportOptions>(); }
} // namespace Crowny

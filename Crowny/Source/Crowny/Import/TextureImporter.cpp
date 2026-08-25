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
        loadOptions.DecodePixels = NormalizeImportExtension(filepath.extension().string()) != "ktx2";
        loadOptions.FlipVertically = true;
        loadOptions.Preserve16Bit = true;
        ImageLoadResult image = ImageLoader::Load(ImageLoadRequest::FromFile(filepath, loadOptions));
        if (!image)
        {
            CW_ENGINE_ERROR("Could not import texture '{}': {}", filepath, image.Error);
            return nullptr;
        }

        if (image.Info.Container == ImageContainerFormat::KTX2)
        {
            TextureDesc desc;
            desc.Width = image.Info.Width;
            desc.Height = image.Info.Height;
            desc.Depth = 1;
            desc.Faces = image.Info.Faces;
            desc.Shape = image.Info.Faces == 6 ? TextureShape::TEXTURE_CUBE : TextureShape::TEXTURE_2D;
            desc.MipLevels = (options->MaxMip == 0 ? image.Info.MipLevels : std::min(image.Info.MipLevels, options->MaxMip + 1u)) - 1u;
            desc.Format = image.Info.PixelFormat;
            desc.sRGB = image.Info.SRGB;
            desc.DebugName = filepath.filename().string();
            Ref<Texture> texture = Texture::CreateDeferred(desc);
            texture->SetEncodedSourceData(image.Info.DiskFormat, desc.Format, std::move(image.SourceData));
            texture->SetCpuCached(options->CpuCached);
            texture->SetName(filepath.filename().string());
            return texture;
        }

        Ref<PixelData> source = image.Pixels;

        TextureFormat sourceFormat = source->GetFormat();
        if (!options->AutomaticFormat && !PixelUtils::IsCompressedFormat(options->Format) && PixelUtils::IsValidFormat(options->Format))
        {
            Ref<PixelData> converted = PixelData::Create(source->GetWidth(), source->GetHeight(), source->GetDepth(), options->Format);
            if (!PixelUtils::ConvertPixels(*source, *converted))
                return nullptr;
            source = converted;
            sourceFormat = options->Format;
        }

        const bool highPrecision = PixelUtils::IsFloatFormat(sourceFormat);
        const bool useBasis = !highPrecision && options->DiskFormat != TextureDiskFormat::None;
        if (highPrecision && options->DiskFormat != TextureDiskFormat::None)
            CW_ENGINE_WARN("High-precision texture '{}' cannot use the LDR Basis modes; storing {} data", filepath,
                           PixelUtils::GetFormatName(sourceFormat));
        TextureDesc desc;
        desc.Width = source->GetWidth();
        desc.Height = source->GetHeight();
        desc.Depth = 1;
        desc.Faces = 1;
        desc.Shape = options->Shape == TextureShape::TEXTURE_CUBE ? TextureShape::TEXTURE_2D : options->Shape;
        desc.Format = sourceFormat;
        desc.sRGB = !highPrecision && options->SRGB;
        desc.DebugName = filepath.filename().string();

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
            CW_ENGINE_ERROR("Could not generate texture mips for '{}': {}", filepath, mipError);
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
                CW_ENGINE_ERROR("Could not Basis-compress texture '{}': {}", filepath, error);
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
        texture->SetName(filepath.filename().string());
        return texture;
    }

    Ref<ImportOptions> TextureImporter::CreateImportOptions() const { return CreateRef<TextureImportOptions>(); }
} // namespace Crowny

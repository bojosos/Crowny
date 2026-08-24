#include "cwpch.h"

#include "Crowny/Renderer/BasisTextureCodec.h"

#include "basis_universal/encoder/basisu_comp.h"
#include "basis_universal/transcoder/basisu_transcoder.h"

namespace Crowny
{
    namespace
    {
        void SetError(String* error, StringView message)
        {
            if (error != nullptr)
                error->assign(message.data(), message.size());
        }

        bool InitializeEncoder()
        {
            static std::once_flag once;
            static bool initialized = false;
            std::call_once(once, [] { initialized = basisu::basisu_encoder_init(false, false); });
            return initialized;
        }

        bool InitializeTranscoder()
        {
            static std::once_flag once;
            std::call_once(once, [] { basist::basisu_transcoder_init(); });
            return true;
        }

        bool InspectTranscoder(basist::ktx2_transcoder& transcoder, const void* data, size_t size, BasisTextureInfo& info,
                               String* error)
        {
            if (data == nullptr || size == 0 || size > std::numeric_limits<uint32_t>::max())
            {
                SetError(error, "Basis texture payload is empty or too large");
                return false;
            }
            InitializeTranscoder();
            if (!transcoder.init(data, static_cast<uint32_t>(size)))
            {
                SetError(error, "Basis texture payload is not a supported KTX2 file");
                return false;
            }
            if (transcoder.get_layers() > 1)
            {
                SetError(error, "Basis texture arrays are not supported yet");
                return false;
            }
            if (transcoder.get_faces() != 1 && transcoder.get_faces() != 6)
            {
                SetError(error, "Basis texture has an unsupported face count");
                return false;
            }
            if (!transcoder.is_etc1s() && !transcoder.is_uastc())
            {
                SetError(error, "Only ETC1S and LDR UASTC KTX2 textures are supported");
                return false;
            }

            info.Width = transcoder.get_width();
            info.Height = transcoder.get_height();
            info.Levels = transcoder.get_levels();
            info.Faces = transcoder.get_faces();
            info.HasAlpha = transcoder.get_has_alpha() != 0;
            info.SRGB = transcoder.get_dfd_transfer_func() == basist::KTX2_KHR_DF_TRANSFER_SRGB;
            info.DiskFormat = transcoder.is_etc1s() ? TextureDiskFormat::ETC1S : TextureDiskFormat::UASTC;
            info.Components = info.HasAlpha ? 4u : 3u;
            const basist::ktx2_df_channel_id channel0 = transcoder.get_dfd_channel_id0();
            const basist::ktx2_df_channel_id channel1 = transcoder.get_dfd_channel_id1();
            if (info.DiskFormat == TextureDiskFormat::UASTC)
            {
                if (channel0 == basist::KTX2_DF_CHANNEL_UASTC_RRR)
                    info.Components = 1;
                else if (channel0 == basist::KTX2_DF_CHANNEL_UASTC_RRRG || channel0 == basist::KTX2_DF_CHANNEL_UASTC_RG)
                    info.Components = 2;
                else if (channel0 == basist::KTX2_DF_CHANNEL_UASTC_RGB)
                    info.Components = 3;
                else if (channel0 == basist::KTX2_DF_CHANNEL_UASTC_RGBA)
                    info.Components = 4;
            }
            else if (channel0 == basist::KTX2_DF_CHANNEL_ETC1S_RRR)
            {
                info.Components = channel1 == basist::KTX2_DF_CHANNEL_ETC1S_GGG ? 2u : 1u;
            }
            return info.Width != 0 && info.Height != 0 && info.Levels != 0;
        }

        bool GetTranscoderFormat(TextureFormat format, basist::transcoder_texture_format& output, int& channel0,
                                 int& channel1)
        {
            channel0 = -1;
            channel1 = -1;
            switch (format)
            {
            case TextureFormat::BC1: output = basist::transcoder_texture_format::cTFBC1_RGB; return true;
            case TextureFormat::BC3: output = basist::transcoder_texture_format::cTFBC3_RGBA; return true;
            case TextureFormat::BC4:
                output = basist::transcoder_texture_format::cTFBC4_R;
                channel0 = 0;
                return true;
            case TextureFormat::BC5:
                output = basist::transcoder_texture_format::cTFBC5_RG;
                channel0 = 0;
                channel1 = 1;
                return true;
            case TextureFormat::BC7: output = basist::transcoder_texture_format::cTFBC7_RGBA; return true;
            case TextureFormat::ETC2_RGB: output = basist::transcoder_texture_format::cTFETC1_RGB; return true;
            case TextureFormat::ETC2_RGBA: output = basist::transcoder_texture_format::cTFETC2_RGBA; return true;
            case TextureFormat::ETC2_R11:
                output = basist::transcoder_texture_format::cTFETC2_EAC_R11;
                channel0 = 0;
                return true;
            case TextureFormat::ETC2_RG11:
                output = basist::transcoder_texture_format::cTFETC2_EAC_RG11;
                channel0 = 0;
                channel1 = 1;
                return true;
            case TextureFormat::ASTC4x4: output = basist::transcoder_texture_format::cTFASTC_4x4_RGBA; return true;
            case TextureFormat::R8:
            case TextureFormat::RG8:
            case TextureFormat::RGB8:
            case TextureFormat::RGBA8: output = basist::transcoder_texture_format::cTFRGBA32; return true;
            default: return false;
            }
        }

        Ref<PixelData> RepackUncompressed(const Vector<uint8_t>& rgba, uint32_t width, uint32_t height,
            TextureFormat targetFormat)
        {
            Ref<PixelData> output = PixelData::Create(width, height, 1, targetFormat);
            const uint32_t components = PixelUtils::GetComponentCount(targetFormat);
            for (size_t pixel = 0; pixel < static_cast<size_t>(width) * height; pixel++)
            {
                for (uint32_t component = 0; component < components; component++)
                    output->GetData()[pixel * components + component] = rgba[pixel * 4u + component];
            }
            return output;
        }

        bool EncodeMipChain(const Vector<const PixelData*>& mipChain, TextureDiskFormat diskFormat, bool sRGB,
                            bool generateMips, Vector<uint8_t>& output, BasisTextureInfo* info, String* error)
        {
            output.clear();
            if (diskFormat != TextureDiskFormat::ETC1S && diskFormat != TextureDiskFormat::UASTC)
            {
                SetError(error, "Basis encoding requires ETC1S or UASTC");
                return false;
            }
            if (mipChain.empty() || !InitializeEncoder())
            {
                SetError(error, mipChain.empty() ? "Basis encoding requires at least one mip level"
                                                 : "Basis Universal encoder initialization failed");
                return false;
            }

            basisu::vector<basisu::image> images;
            images.reserve(mipChain.size());
            uint32_t expectedWidth = mipChain.front()->GetWidth();
            uint32_t expectedHeight = mipChain.front()->GetHeight();
            for (const PixelData* source : mipChain)
            {
                if (source == nullptr || !source->IsValid() || source->GetDepth() != 1 ||
                    PixelUtils::IsCompressedFormat(source->GetFormat()) || PixelUtils::IsFloatFormat(source->GetFormat()) ||
                    source->GetWidth() != expectedWidth || source->GetHeight() != expectedHeight)
                {
                    SetError(error, "Basis encoding received an invalid LDR mip chain");
                    return false;
                }

                PixelData rgba(source->GetWidth(), source->GetHeight(), 1, TextureFormat::RGBA8);
                rgba.AllocateInternalBuffer();
                if (!PixelUtils::ConvertPixels(*source, rgba))
                {
                    SetError(error, "Could not convert a texture mip to RGBA8 for Basis encoding");
                    return false;
                }
                images.emplace_back(rgba.GetData(), rgba.GetWidth(), rgba.GetHeight(), 4);
                expectedWidth = std::max(expectedWidth / 2u, 1u);
                expectedHeight = std::max(expectedHeight / 2u, 1u);
            }

            uint32_t flags = basisu::cFlagKTX2 | basisu::cFlagThreaded;
            if (sRGB)
                flags |= basisu::cFlagSRGB;
            if (generateMips && images.size() == 1)
                flags |= basisu::cFlagGenMipsClamp;

            const basist::basis_tex_format mode = diskFormat == TextureDiskFormat::ETC1S
                                                    ? basist::basis_tex_format::cETC1S
                                                    : basist::basis_tex_format::cUASTC4x4;
            if (diskFormat == TextureDiskFormat::ETC1S)
                flags |= 192u;
            else
            {
                flags |= basisu::cPackUASTCLevelDefault;
                flags |= basisu::cFlagKTX2UASTCSuperCompression;
            }

            size_t encodedSize = 0;
            void* encoded = basisu::basis_compress(mode, images, flags, 0.0f, &encodedSize);
            if (encoded == nullptr || encodedSize == 0)
            {
                SetError(error, "Basis Universal failed to encode the texture");
                return false;
            }
            output.resize(encodedSize);
            std::memcpy(output.data(), encoded, encodedSize);
            basisu::basis_free_data(encoded);

            if (info != nullptr && !BasisTextureCodec::Inspect(output.data(), output.size(), *info, error))
            {
                output.clear();
                return false;
            }
            return true;
        }
    } // namespace

    bool BasisTextureCodec::Encode(const PixelData& source, TextureDiskFormat diskFormat, bool sRGB, bool generateMips,
                                   Vector<uint8_t>& output, BasisTextureInfo* info, String* error)
    {
        return EncodeMipChain({ &source }, diskFormat, sRGB, generateMips, output, info, error);
    }

    bool BasisTextureCodec::Encode(const Vector<Ref<PixelData>>& mipChain, TextureDiskFormat diskFormat, bool sRGB,
                                   Vector<uint8_t>& output, BasisTextureInfo* info, String* error)
    {
        Vector<const PixelData*> levels;
        levels.reserve(mipChain.size());
        for (const Ref<PixelData>& mip : mipChain)
            levels.push_back(mip.get());
        return EncodeMipChain(levels, diskFormat, sRGB, false, output, info, error);
    }

    bool BasisTextureCodec::Inspect(const void* data, size_t size, BasisTextureInfo& info, String* error)
    {
        info = {};
        basist::ktx2_transcoder transcoder;
        return InspectTranscoder(transcoder, data, size, info, error);
    }

    TextureFormat BasisTextureCodec::SelectTarget(const BasisTextureInfo& info, TextureFormat sourceFormat,
                                                  const RenderCapabilities& capabilities)
    {
        const bool bc = capabilities.HasCapability(CW_TEXTURE_COMPRESSION_BC);
        const bool bptc = capabilities.HasCapability(CW_TEXTURE_COMPRESSION_BPTC);
        const bool etc2 = capabilities.HasCapability(CW_TEXTURE_COMPRESSION_ETC2);
        const bool astc = capabilities.HasCapability(CW_TEXTURE_COMPRESSION_ASTC);
        const uint32_t components = PixelUtils::GetComponentCount(sourceFormat);
        if (components == 1)
        {
            if (bc)
                return TextureFormat::BC4;
            if (etc2)
                return TextureFormat::ETC2_R11;
            if (astc)
                return TextureFormat::ASTC4x4;
            return TextureFormat::R8;
        }
        if (components == 2)
        {
            if (bc)
                return TextureFormat::BC5;
            if (etc2)
                return TextureFormat::ETC2_RG11;
            if (astc)
                return TextureFormat::ASTC4x4;
            return TextureFormat::RG8;
        }
        if (bptc && info.DiskFormat == TextureDiskFormat::UASTC)
            return TextureFormat::BC7;
        if (bc)
            return info.HasAlpha ? TextureFormat::BC3 : TextureFormat::BC1;
        if (astc)
            return TextureFormat::ASTC4x4;
        if (etc2)
            return info.HasAlpha ? TextureFormat::ETC2_RGBA : TextureFormat::ETC2_RGB;
        return TextureFormat::RGBA8;
    }

    bool BasisTextureCodec::Transcode(const void* data, size_t size, TextureFormat sourceFormat, TextureFormat targetFormat,
                                      uint32_t maximumLevels, BasisTextureTranscodeResult& output, String* error)
    {
        output = {};
        basist::ktx2_transcoder transcoder;
        if (!InspectTranscoder(transcoder, data, size, output.Info, error))
            return false;

        basist::transcoder_texture_format transcodeFormat;
        int channel0 = -1;
        int channel1 = -1;
        if (!GetTranscoderFormat(targetFormat, transcodeFormat, channel0, channel1))
        {
            SetError(error, "Requested GPU texture format cannot be produced by Basis Universal");
            return false;
        }
        if (!transcoder.start_transcoding())
        {
            SetError(error, "Basis Universal could not start transcoding the KTX2 payload");
            return false;
        }

        const uint32_t levels = maximumLevels == 0 ? output.Info.Levels : std::min(maximumLevels, output.Info.Levels);
        output.Info.Levels = levels;
        output.Format = targetFormat;
        output.Subresources.reserve(static_cast<size_t>(levels) * output.Info.Faces);
        const bool compressed = PixelUtils::IsCompressedFormat(targetFormat);
        const bool directRgba = targetFormat == TextureFormat::RGBA8;

        for (uint32_t mip = 0; mip < levels; mip++)
        {
            const uint32_t width = std::max(output.Info.Width >> mip, 1u);
            const uint32_t height = std::max(output.Info.Height >> mip, 1u);
            for (uint32_t face = 0; face < output.Info.Faces; face++)
            {
                TextureFormat storageFormat = compressed || directRgba ? targetFormat : TextureFormat::RGBA8;
                Ref<PixelData> pixels = PixelData::Create(width, height, 1, storageFormat);
                const uint32_t outputUnits = compressed
                                               ? static_cast<uint32_t>(pixels->GetSize() / PixelUtils::GetBlockSize(targetFormat))
                                               : width * height;
                if (!transcoder.transcode_image_level(mip, 0, face, pixels->GetData(), outputUnits, transcodeFormat, 0, 0, 0,
                                                      channel0, channel1))
                {
                    SetError(error, "Basis Universal failed to transcode a texture subresource");
                    output = {};
                    return false;
                }

                if (!compressed && !directRgba)
                {
                    Vector<uint8_t> rgba(pixels->GetData(), pixels->GetData() + pixels->GetSize());
                    pixels = RepackUncompressed(rgba, width, height, targetFormat);
                }
                output.Subresources.push_back(std::move(pixels));
            }
        }
        return true;
    }
} // namespace Crowny

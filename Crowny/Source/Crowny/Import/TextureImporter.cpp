#include "cwpch.h"

#include "Crowny/Import/TextureImporter.h"

#include "Crowny/Common//FileSystem.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/RenderAPI/Texture.h"

#include "basis_universal/encoder/basisu_comp.h"

#include <stb_image.h>

namespace Crowny
{

    bool TextureImporter::IsExtensionSupportedStatic(const String& ext)
    {
        String lower = ext;
        StringUtils::ToLower(lower);
        return lower == "png" || lower == "jpeg" || lower == "psd" || lower == "gif" || lower == "tga" || lower == "bmp" || lower == "hdr" ||
               lower == "pic" || lower == "ppm" || lower == "pgm" || lower == "jpg";
    }

    bool TextureImporter::IsExtensionSupported(const String& ext) const { return IsExtensionSupportedStatic(ext); }

    bool TextureImporter::IsMagicNumSupported(uint8_t* num, uint32_t numSize) const { return false; }

    // Importer currently only supports 32-bit 1,3,4-channel images
    // Going to switch to FreeImage soon
    Ref<Asset> TextureImporter::Import(const Path& filepath, Ref<const ImportOptions> importOptions)
    {
        Ref<const TextureImportOptions> textureImportOptions = std::static_pointer_cast<const TextureImportOptions>(importOptions);
        int width, height, channels;
        stbi_set_flip_vertically_on_load(1);

        std::vector<uint8_t> data;
        Ref<DataStream> stream = FileSystem::OpenFile(filepath);
        data.resize(stream->Size());
        stream->Read(data.data(), data.size());
        stream->Close();
        const bool is16 = stbi_is_16_bit_from_memory(data.data(), (int)data.size());
        uint8_t* rawPixelData;

        if (is16)
            rawPixelData = (uint8_t*)stbi_load_16_from_memory(data.data(), (int)data.size(), &width, &height, &channels, 0);
        else
            rawPixelData = stbi_load_from_memory(data.data(), (int)data.size(), &width, &height, &channels, 0);

        if (rawPixelData == nullptr)
        {
            CW_ENGINE_INFO(stbi_failure_reason());
            return nullptr;
        }

        TextureFormat format = TextureFormat::RGBA8;
        if (channels == 1)
            format = TextureFormat::R8;
        else if (channels == 3)
            format = /*is16 ? TextureFormat::RGB16 :*/ TextureFormat::RGB8;
        else if (channels == 4)
            format = /*is16 ? TextureFormat::RGBA16 :*/ TextureFormat::RGBA8;
        else
            CW_ENGINE_ASSERT("2-Channel textures are not supported");

        if (textureImportOptions->DiskFormat != TextureDiskFormat::None && false)
        {
            static bool basisInitialied = false;
            if (!basisInitialied)
                basisu::basisu_encoder_init(false, false);
            basisInitialied = true;
            const basist::basis_tex_format basisFormat =
              (textureImportOptions->DiskFormat == TextureDiskFormat::ETC1S) ? basist::basis_tex_format::cETC1S : basist::basis_tex_format::cUASTC4x4;
            const uint32_t qualityLevel = 128;
            const uint32_t flags =
              basisu::cFlagGenMipsClamp | basisu::cFlagThreaded | basisu::cFlagDebug | basisu::cFlagPrintStats | basisu::cFlagPrintStatus;
            basisu::vector<basisu::image> images;
            images.push_back(basisu::image(rawPixelData, width, height, channels));
            size_t size = 0;
            void* ktx2Data = basisu::basis_compress(basisFormat, images, qualityLevel | flags, 0.0f, &size);
            if (!ktx2Data)
            {
                CW_ENGINE_ERROR("Failed to compress texture to basis format: {0}", filepath);
                stbi_image_free(rawPixelData);
                return nullptr;
            };
            CW_ENGINE_INFO("Basis compression stats for texture {0}: ({1}, {2}) -> {3}", filepath, data.size(),
                           width * height * channels * sizeof(float), size);
            FileSystem::WriteFile("C:\\dev\\" + filepath.filename().replace_extension(".ktx2").string(), (uint8_t*)ktx2Data, size);
            basisu::basis_free_data(ktx2Data);
            Ref<DataStream> stream = FileSystem::OpenFile("C:\\dev\\" + filepath.filename().replace_extension(".ktx2").string());
            Vector<uint8_t> ktxData = stream->ReadAll();

            

            TextureParameters params;
            params.Format = TextureFormat::BC7;
            params.Width = width;
            params.Height = height;
            Ref<Texture> texture = Texture::Create(params);
            PixelData pixelData(width, height, 1, TextureFormat::BC7);
            pixelData.SetBuffer(data.data());
            texture->WriteData(pixelData);
            texture->SetName(filepath.filename().string());
            stbi_image_free(rawPixelData);
            return texture;
        }
        else
        {
            TextureParameters params;
            params.Width = width;
            params.Height = height;
            if (textureImportOptions->AutomaticFormat)
                params.Format = format;
            else
                params.Format = textureImportOptions->Format;

            PixelData pixelData(width, height, 1, format);
            Ref<Texture> texture = Texture::Create(params);
            pixelData.SetBuffer(rawPixelData);
            texture->WriteData(pixelData);
            texture->SetName(filepath.filename().string());
            stbi_image_free(rawPixelData);
            return texture;
        }
    }

    Ref<ImportOptions> TextureImporter::CreateImportOptions() const { return CreateRef<TextureImportOptions>(); }

} // namespace Crowny
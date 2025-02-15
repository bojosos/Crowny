
#include "cwpch.h"

#include "Crowny/Import/TextureImporter.h"

#include "Crowny/Common//FileSystem.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/RenderAPI/Texture.h"

#include <stb_image.h>

namespace Crowny
{

    bool TextureImporter::IsExtensionSupported(const String& ext) const
    {
        String lower = ext;
        StringUtils::ToLower(lower);
        return lower == "png" || lower == "jpeg" || lower == "psd" || lower == "gif" || lower == "tga" ||
               lower == "bmp" || lower == "hdr";
    }

    bool TextureImporter::IsMagicNumSupported(uint8_t* num, uint32_t numSize) const { return true; }

    // Importer currently only supports 32-bit 1,3,4-channel images
    // Going to switch to FreeImage soon
    Ref<Asset> TextureImporter::Import(const Path& filepath, Ref<const ImportOptions> importOptions)
    {
        Ref<const TextureImportOptions> textureImportOptions =
          std::static_pointer_cast<const TextureImportOptions>(importOptions);
        int width, height, channels;
        stbi_set_flip_vertically_on_load(1);

        // auto [loaded, size] = VirtualFileSystem::Get()->ReadFile(filepath);
        std::vector<uint8_t> data;
        Ref<DataStream> stream = FileSystem::OpenFile(filepath);
        data.resize(stream->Size());
        stream->Read(data.data(), data.size());
        stream->Close();
        bool is16 = stbi_is_16_bit_from_memory(data.data(), (int)data.size());
        uint8_t* rawPixelData;

        if (is16)
            rawPixelData =
              (uint8_t*)stbi_load_16_from_memory(data.data(), (int)data.size(), &width, &height, &channels, 0);
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

    Ref<ImportOptions> TextureImporter::CreateImportOptions() const { return CreateRef<TextureImportOptions>(); }

} // namespace Crowny
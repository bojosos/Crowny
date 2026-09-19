#include "cwpch.h"

#include "Crowny/Assets/AssetCodecs.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/BasisTextureCodec.h"
#include "Crowny/Renderer/SpriteAtlas.h"
#include "Crowny/Serialization/CerealDataStreamArchive.h"

namespace Crowny
{
    namespace
    {
        Ref<PixelData> ReadAtlasSourcePixels(const AssetHandle<Texture>& texture, String& error)
        {
            Ref<Texture> source = texture.GetInternalPtr();
            if (!source->HasEncodedSourceData() && PixelUtils::IsCompressedFormat(source->GetFormat()))
            {
                // The live texture may have discarded its Basis payload. Read a
                // private cooked copy; do not replace shared runtime handles.
                Path path;
                if (AssetManager::Get().GetAssetPath(texture.GetUUID(), path))
                {
                    const auto stream = FileSystem::OpenFile(path);
                    if (stream)
                    {
                        BinaryDataStreamInputArchive archive(stream);
                        Ref<Asset> asset;
                        archive(asset);
                        if (asset && asset->GetAssetType() == AssetType::Texture)
                            source = StaticRefCast<Texture>(asset);
                    }
                }
            }
            if (source->HasEncodedSourceData())
            {
                const auto& encoded = source->GetEncodedSourceData();
                BasisTextureTranscodeResult decoded;
                if (!BasisTextureCodec::Transcode(encoded.data(), encoded.size(), source->GetSourceFormat(), TextureFormat::RGBA8, 1, decoded,
                                                  &error) ||
                    decoded.Subresources.empty())
                    return nullptr;
                return decoded.Subresources[0].Pixels;
            }
            if (PixelUtils::IsCompressedFormat(source->GetFormat()))
            {
                error = "The compressed source texture needs its cooked Basis payload. Reimport the source texture.";
                return nullptr;
            }
            auto pixels = source->AllocatePixelData(0, 0);
            source->ReadData(*pixels);
            auto rgba = PixelData::Create(source->GetWidth(), source->GetHeight(), TextureFormat::RGBA8);
            if (!PixelUtils::ConvertPixels(*pixels, *rgba))
            {
                error = "The source texture cannot be converted to RGBA8.";
                return nullptr;
            }
            return rgba;
        }
    } // namespace

    bool SpriteAtlas::SetData(const SpriteAtlasData& data)
    {
        if (data == m_Data)
            return true;
        const auto previous = m_Data;
        m_Data = data;
        if (Rebuild())
            return true;
        m_Data = previous;
        return false;
    }

    bool SpriteAtlas::Rebuild()
    {
        m_LastError.clear();
        SpriteAtlasSettings settings;
        settings.PageSize = m_Data.PageSize;
        settings.MipLevels = m_Data.MipLevels;
        const auto fail = [&](const String& message) {
            m_LastError = message;
            return false;
        };
        if (m_Data.Sprites.size() > 65536 || settings.PageSize < 4 || settings.PageSize > 8192 || (settings.PageSize & (settings.PageSize - 1)) ||
            settings.MipLevels == 0 || settings.MipLevels > PixelUtils::GetMaxMipCount(settings.PageSize, settings.PageSize))
            return fail("Invalid atlas settings.");
        auto* manager = AssetManager::TryGet();
        if (!manager)
            return fail("Atlas authoring requires an asset manager.");
        try
        {
            Vector<SpriteAtlasInput> inputs;
            UnorderedMap<UUID, Ref<PixelData>> sources;
            for (const auto& id : m_Data.Sprites)
            {
                if (id.Empty())
                    return fail("Assign a sprite to every atlas entry.");
                auto asset = manager->GetAssetHandle(id);
                if (!asset)
                {
                    Path path;
                    AssetFileHeader header;
                    if (!manager->GetAssetPath(id, path) || !PeekAssetHeader(path, header) || header.Type != AssetType::Sprite)
                        return fail("Sprite source is missing or has the wrong asset type: " + id.ToString());
                    asset = manager->LoadFromUUID(id, false);
                }
                if (!asset || asset->GetAssetType() != AssetType::Sprite)
                    return fail("Sprite source is missing or has the wrong asset type: " + id.ToString());
                const auto sprite = static_asset_cast<Sprite>(asset);
                const auto& texture = sprite->GetTexture();
                if (!texture || texture->GetDesc().Shape != TextureShape::TEXTURE_2D || texture->GetDepth() != 1)
                    return fail("Sprite source requires an available 2D texture: " + id.ToString());
                auto& pixels = sources[texture.GetUUID()];
                if (!pixels)
                    pixels = ReadAtlasSourcePixels(texture, m_LastError);
                if (!pixels)
                    return false;
                const auto uv = sprite->GetData().UvRect;
                const glm::vec4 texels = uv * glm::vec4(pixels->GetWidth(), pixels->GetHeight(), pixels->GetWidth(), pixels->GetHeight());
                const auto rect = glm::round(texels);
                if (glm::any(glm::greaterThan(glm::abs(texels - rect), glm::vec4(0.001f))) || rect.z <= rect.x || rect.w <= rect.y)
                    return fail("Atlas source regions must end on source pixel boundaries.");
                SpriteAtlasInput input;
                input.SpriteId = id;
                input.Metadata = sprite->GetData();
                input.SRGB = texture->GetDesc().sRGB;
                input.Pixels =
                  PixelData::Create(static_cast<uint32_t>(rect.z - rect.x), static_cast<uint32_t>(rect.w - rect.y), TextureFormat::RGBA8);
                for (uint32_t y = 0; y < input.Pixels->GetHeight(); ++y)
                    std::memcpy(input.Pixels->GetData() + y * input.Pixels->GetRowPitch(),
                                pixels->GetData() + (y + static_cast<uint32_t>(rect.y)) * pixels->GetRowPitch() + static_cast<uint32_t>(rect.x) * 4,
                                input.Pixels->GetWidth() * 4);
                inputs.push_back(std::move(input));
            }
            return Build(settings, inputs, &m_LastError);
        }
        catch (const std::exception& exception)
        {
            return fail(exception.what());
        }
    }

    bool SpriteAtlas::Build(const SpriteAtlasSettings& settings, const Vector<SpriteAtlasInput>& inputs, String* error)
    {
        SpriteAtlasBuild packed;
        if (!BuildSpriteAtlas(settings, inputs, packed, error))
            return false;
        auto* manager = AssetManager::TryGet();
        if (!manager)
        {
            if (error)
                *error = "Atlas publication requires an asset manager.";
            return false;
        }
        Vector<AssetHandle<Texture>> pages;
        for (auto& page : packed.Pages)
        {
            TextureDesc desc;
            desc.Width = desc.Height = settings.PageSize;
            desc.MipLevels = settings.MipLevels - 1;
            desc.sRGB = page.SRGB;
            desc.DebugName = GetName() + " atlas page " + std::to_string(pages.size());
            auto texture = Texture::CreateDeferred(desc);
            if (!texture)
            {
                if (error)
                    *error = "Atlas page allocation failed.";
                return false;
            }
            Vector<TextureSubresourceData> resources;
            for (uint32_t mip = 0; mip < page.Mips.size(); ++mip)
                resources.push_back({ mip, 0, std::move(page.Mips[mip]) });
            texture->SetPendingSubresources(std::move(resources));
            texture->Init();
            pages.push_back(static_asset_cast<Texture>(manager->CreateAssetHandle(texture)));
        }
        m_Entries = std::move(packed.Entries);
        m_Pages = std::move(pages);
        m_Data.PageSize = settings.PageSize;
        m_Data.MipLevels = settings.MipLevels;
        m_Data.Sprites.clear();
        for (const auto& input : inputs)
            m_Data.Sprites.push_back(input.SpriteId);
        return true;
    }

    const SpriteAtlasEntry* SpriteAtlas::Find(const UUID& spriteId) const
    {
        const auto it =
          std::lower_bound(m_Entries.begin(), m_Entries.end(), spriteId, [](const auto& entry, const UUID& id) { return entry.SpriteId < id; });
        return it != m_Entries.end() && it->SpriteId == spriteId ? &*it : nullptr;
    }

    const AssetHandle<Texture>& SpriteAtlas::GetTexture(const UUID& spriteId) const
    {
        static const AssetHandle<Texture> missing;
        const auto* entry = Find(spriteId);
        return entry && entry->Page < m_Pages.size() ? m_Pages[entry->Page] : missing;
    }

    bool BuildSpriteAtlas(const SpriteAtlasSettings& settings, const Vector<SpriteAtlasInput>& inputs, SpriteAtlasBuild& output, String* error)
    {
        const auto fail = [&](const char* message) {
            if (error)
                *error = message;
            return false;
        };
        if (error)
            error->clear();
        const uint32_t size = settings.PageSize;
        if (size < 4 || size > 8192 || (size & (size - 1)) || settings.MipLevels == 0 ||
            settings.MipLevels > PixelUtils::GetMaxMipCount(size, size) || settings.MaxPages == 0 || inputs.size() > 65536)
            return fail("Invalid atlas page size, mip count, page limit, or sprite count.");
        const uint32_t padding = 1u << (settings.MipLevels - 1);
        const auto align = [&](uint32_t value) { return (value + padding - 1) & ~(padding - 1); };
        uint64_t pageBytes = 0;
        for (uint32_t mip = 0; mip < settings.MipLevels; ++mip)
            pageBytes += uint64_t(size >> mip) * (size >> mip) * 4;
        Vector<const SpriteAtlasInput*> sorted;
        sorted.reserve(inputs.size());
        for (const auto& input : inputs)
        {
            if (input.SpriteId.Empty() || !input.Metadata.IsValid() || !input.Pixels || !input.Pixels->IsValid() || input.Pixels->GetDepth() != 1 ||
                input.Pixels->GetFormat() != TextureFormat::RGBA8)
                return fail("Atlas inputs require a sprite identity, valid metadata, and RGBA8 image pixels.");
            if (uint64_t(input.Pixels->GetWidth()) + 2ull * padding > size || uint64_t(input.Pixels->GetHeight()) + 2ull * padding > size)
                return fail("A sprite and its mip padding exceed the atlas page size.");
            sorted.push_back(&input);
        }
        std::sort(sorted.begin(), sorted.end(), [](const auto* a, const auto* b) { return a->SpriteId < b->SpriteId; });
        for (size_t i = 1; i < sorted.size(); ++i)
            if (sorted[i - 1]->SpriteId == sorted[i]->SpriteId)
                return fail("An atlas cannot contain the same sprite identity twice.");

        SpriteAtlasBuild result;
        struct Shelf
        {
            uint32_t X = 0, Y = 0, Height = 0;
        };
        Vector<Shelf> shelves;
        for (const auto* input : sorted)
        {
            const uint32_t width = input->Pixels->GetWidth(), height = input->Pixels->GetHeight();
            const uint32_t paddedWidth = align(width + 2 * padding), paddedHeight = align(height + 2 * padding);
            uint32_t page = 0, x = 0, y = 0;
            for (; page < result.Pages.size(); ++page)
            {
                if (result.Pages[page].SRGB != input->SRGB)
                    continue;
                const auto& shelf = shelves[page];
                x = shelf.X;
                y = shelf.Y;
                if (x + paddedWidth > size)
                {
                    x = 0;
                    y += shelf.Height;
                }
                if (y + paddedHeight <= size)
                    break;
            }
            if (page == result.Pages.size())
            {
                if (page >= settings.MaxPages || (uint64_t(page) + 1) > settings.MaxBytes / pageBytes)
                    return fail("Atlas page or memory limit exceeded.");
                SpriteAtlasPage next;
                next.SRGB = input->SRGB;
                for (uint32_t mip = 0; mip < settings.MipLevels; ++mip)
                {
                    auto pixels = PixelData::Create(size >> mip, size >> mip, TextureFormat::RGBA8);
                    std::memset(pixels->GetData(), 0, pixels->GetSize());
                    next.Mips.push_back(std::move(pixels));
                }
                result.Pages.push_back(std::move(next));
                shelves.emplace_back();
                x = y = 0;
            }
            auto& shelf = shelves[page];
            shelf.Height = y == shelf.Y ? std::max(shelf.Height, paddedHeight) : paddedHeight;
            shelf.X = x + paddedWidth;
            shelf.Y = y;

            // Filter each extruded rectangle separately; adjacent sprites never
            // contribute to its mip chain, including transparent edge colors.
            auto padded = PixelData::Create(paddedWidth, paddedHeight, TextureFormat::RGBA8);
            for (uint32_t py = 0; py < paddedHeight; ++py)
                for (uint32_t px = 0; px < paddedWidth; ++px)
                {
                    const uint32_t sx = std::clamp<int64_t>(int64_t(px) - padding, 0, width - 1);
                    const uint32_t sy = std::clamp<int64_t>(int64_t(py) - padding, 0, height - 1);
                    std::memcpy(padded->GetData() + py * padded->GetRowPitch() + px * 4,
                                input->Pixels->GetData() + sy * input->Pixels->GetRowPitch() + sx * 4, 4);
                }
            TextureMipGenerationOptions options;
            options.Filter = TextureMipFilter::Box;
            options.MaxLevels = settings.MipLevels;
            options.SRGB = input->SRGB;
            Vector<Ref<PixelData>> mips;
            if (!PixelUtils::GenerateMipChain(*padded, options, mips, error))
                return false;
            for (uint32_t mip = 0; mip < settings.MipLevels; ++mip)
            {
                auto& target = *result.Pages[page].Mips[mip];
                const auto& source = *mips[mip];
                for (uint32_t row = 0; row < source.GetHeight(); ++row)
                    std::memcpy(target.GetData() + ((y >> mip) + row) * target.GetRowPitch() + (x >> mip) * 4,
                                source.GetData() + row * source.GetRowPitch(), source.GetWidth() * 4);
            }
            SpriteAtlasEntry entry;
            entry.SpriteId = input->SpriteId;
            entry.Metadata = input->Metadata;
            entry.Page = page;
            auto dimensions = input->Metadata.OriginalSize;
            if (dimensions.x == 0)
                dimensions.x = static_cast<float>(width);
            if (dimensions.y == 0)
                dimensions.y = static_cast<float>(height);
            entry.Geometry = { dimensions / input->Metadata.PixelsPerUnit, input->Metadata.Pivot,
                               glm::vec4(x + padding, y + padding, x + padding + width, y + padding + height) / float(size) };
            if (!entry.Geometry.IsValid())
                return fail("Sprite dimensions or pixel scale overflow atlas geometry.");
            result.Entries.push_back(entry);
        }
        output = std::move(result);
        return true;
    }
} // namespace Crowny

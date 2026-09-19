#pragma once

#include "Crowny/Renderer/Sprite.h"
#include "Crowny/Utils/PixelUtils.h"

namespace Crowny
{
    struct SpriteAtlasSettings
    {
        uint32_t PageSize = 1024;
        // Includes the base level. Padding keeps every configured mip isolated.
        uint32_t MipLevels = 4;
        uint32_t MaxPages = 64;
        uint64_t MaxBytes = 256ull * 1024 * 1024;
    };

    struct SpriteAtlasInput
    {
        UUID SpriteId;
        SpriteData Metadata;
        // The untrimmed sprite region, in source pixel orientation.
        Ref<PixelData> Pixels;
        bool SRGB = true;
    };

    struct SpriteAtlasEntry
    {
        UUID SpriteId;
        SpriteData Metadata;
        SpriteGeometry Geometry;
        uint32_t Page = 0;
    };

    struct SpriteAtlasPage
    {
        bool SRGB = true;
        Vector<Ref<PixelData>> Mips;
    };

    struct SpriteAtlasBuild
    {
        Vector<SpriteAtlasEntry> Entries;
        Vector<SpriteAtlasPage> Pages;
    };

    struct SpriteAtlasData
    {
        Vector<UUID> Sprites;
        uint32_t PageSize = 1024;
        uint32_t MipLevels = 4;
        bool operator==(const SpriteAtlasData&) const = default;
    };

    // CPU-only packing and filtering. Suitable for importer workers; publication
    // and GPU uploads belong to the caller. Failure leaves output untouched.
    bool BuildSpriteAtlas(const SpriteAtlasSettings& settings, const Vector<SpriteAtlasInput>& inputs, SpriteAtlasBuild& output,
                          String* error = nullptr);

    class SpriteAtlas : public Asset
    {
    public:
        AssetType GetAssetType() const override { return AssetType::SpriteAtlas; }
        static AssetType GetStaticType() { return AssetType::SpriteAtlas; }
        // Builds and uploads on the asset/render thread. Existing pages remain
        // usable if packing fails; snapshots retain replaced page resources.
        bool Build(const SpriteAtlasSettings& settings, const Vector<SpriteAtlasInput>& inputs, String* error = nullptr);
        const SpriteAtlasData& GetData() const { return m_Data; }
        // Authoring path: resolve source sprites, crop their pixels, and repack.
        // Missing sources fail explicitly and preserve the previous atlas.
        bool SetData(const SpriteAtlasData& data);
        bool Rebuild();
        const String& GetLastError() const { return m_LastError; }
        const Vector<SpriteAtlasEntry>& GetEntries() const { return m_Entries; }
        const Vector<AssetHandle<Texture>>& GetPages() const { return m_Pages; }
        const SpriteAtlasEntry* Find(const UUID& spriteId) const;
        const AssetHandle<Texture>& GetTexture(const UUID& spriteId) const;

    private:
        CW_SERIALIZABLE(SpriteAtlas);
        Vector<SpriteAtlasEntry> m_Entries;
        Vector<AssetHandle<Texture>> m_Pages;
        SpriteAtlasData m_Data;
        String m_LastError;
    };
} // namespace Crowny

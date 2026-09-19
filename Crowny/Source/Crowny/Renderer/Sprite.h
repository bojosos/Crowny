#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include <glm/glm.hpp>

namespace Crowny
{
    class Texture;

    struct SpriteData
    {
        UUID TextureId;
        glm::vec4 UvRect{ 0, 0, 1, 1 };
        glm::vec2 Pivot{ 0.5f };
        // Zero dimensions derive the original size from the texture region.
        glm::vec2 OriginalSize{ 0 };
        float PixelsPerUnit = 100.0f;
        // Left, bottom, right, top, in original image pixels.
        glm::vec4 Borders{ 0 };
        bool operator==(const SpriteData&) const = default;
        bool IsValid() const;
    };

    struct SpriteGeometry
    {
        glm::vec2 Size{ 1 };
        glm::vec2 Pivot{ 0.5f };
        glm::vec4 UvRect{ 0, 0, 1, 1 };
        bool IsValid() const;
        glm::mat4 QuadTransform(const glm::mat4& world, bool flipX, bool flipY) const;
        glm::vec4 QuadUvRect(bool flipX, bool flipY) const;
    };

    class Sprite : public Asset
    {
    public:
        AssetType GetAssetType() const override { return AssetType::Sprite; }
        static AssetType GetStaticType() { return AssetType::Sprite; }
        const SpriteData& GetData() const { return m_Data; }
        // Invalid edits leave the asset unchanged. Missing texture UUIDs are retained.
        bool SetData(const SpriteData& data);
        const AssetHandle<Texture>& GetTexture() const;
        SpriteGeometry GetGeometry() const;

    private:
        CW_SERIALIZABLE(Sprite);
        SpriteData m_Data;
        AssetHandle<Texture> m_Texture;
    };
} // namespace Crowny

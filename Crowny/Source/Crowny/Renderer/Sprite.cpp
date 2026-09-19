#include "cwpch.h"

#include "Crowny/Assets/AssetCodecs.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/Sprite.h"

namespace Crowny
{
    bool SpriteGeometry::IsValid() const
    {
        const glm::vec2 center = (glm::vec2(0.5f) - Pivot) * Size;
        return std::isfinite(Size.x) && std::isfinite(Size.y) && Size.x > 0 && Size.y > 0 && std::isfinite(center.x) && std::isfinite(center.y) &&
               std::isfinite(UvRect.x) && std::isfinite(UvRect.y) && std::isfinite(UvRect.z) && std::isfinite(UvRect.w) && UvRect.x >= 0 &&
               UvRect.y >= 0 && UvRect.z <= 1 && UvRect.w <= 1 && UvRect.z > UvRect.x && UvRect.w > UvRect.y;
    }

    glm::mat4 SpriteGeometry::QuadTransform(const glm::mat4& world, bool flipX, bool flipY) const
    {
        const glm::vec2 center = (glm::vec2(0.5f) - Pivot) * Size * glm::vec2(flipX ? -1.0f : 1.0f, flipY ? -1.0f : 1.0f);
        glm::mat4 quad = world;
        quad[3] += world[0] * center.x + world[1] * center.y;
        quad[0] *= Size.x;
        quad[1] *= Size.y;
        return quad;
    }

    glm::vec4 SpriteGeometry::QuadUvRect(bool flipX, bool flipY) const
    {
        return { flipX ? UvRect.z : UvRect.x, flipY ? UvRect.w : UvRect.y, flipX ? UvRect.x : UvRect.z, flipY ? UvRect.y : UvRect.w };
    }

    bool SpriteData::IsValid() const
    {
        if (!SpriteGeometry{ glm::vec2(1), Pivot, UvRect }.IsValid() || !std::isfinite(PixelsPerUnit) || PixelsPerUnit <= 0 ||
            !std::isfinite(OriginalSize.x) || !std::isfinite(OriginalSize.y) || OriginalSize.x < 0 || OriginalSize.y < 0)
            return false;
        for (int index = 0; index < 4; ++index)
            if (!std::isfinite(Borders[index]) || Borders[index] < 0)
                return false;
        return (OriginalSize.x == 0 || double(Borders.x) + Borders.z <= OriginalSize.x) &&
               (OriginalSize.y == 0 || double(Borders.y) + Borders.w <= OriginalSize.y);
    }

    bool Sprite::SetData(const SpriteData& data)
    {
        if (!data.IsValid())
            return false;
        AssetHandle<Texture> texture = m_Texture;
        if (data.TextureId != m_Data.TextureId || (!GetTexture() && !data.TextureId.Empty()))
        {
            texture = nullptr;
            if (auto* manager = AssetManager::TryGet(); manager && !data.TextureId.Empty())
            {
                auto loaded = manager->GetAssetHandle(data.TextureId);
                if (!loaded)
                {
                    // Reject known non-textures before loading their dependencies. This
                    // also prevents a malformed sprite from recursively loading itself.
                    Path path;
                    AssetFileHeader header;
                    if (manager->GetAssetPath(data.TextureId, path) && PeekAssetHeader(path, header) && header.Type != AssetType::Texture)
                        return false;
                    const auto candidate = manager->LoadFromUUID(data.TextureId, false);
                    if (candidate)
                        loaded = candidate;
                }
                if (loaded && loaded->GetAssetType() != AssetType::Texture)
                    return false;
                texture = static_asset_cast<Texture>(loaded);
            }
        }
        m_Data = data;
        m_Texture = std::move(texture);
        return true;
    }

    const AssetHandle<Texture>& Sprite::GetTexture() const
    {
        static const AssetHandle<Texture> missing;
        // A missing UUID may later resolve to a different asset kind after reimport.
        const auto& handle = m_Texture.GetHandleData();
        return handle && handle->m_Ptr && handle->m_Ptr->GetAssetType() != AssetType::Texture ? missing : m_Texture;
    }

    SpriteGeometry Sprite::GetGeometry() const
    {
        glm::vec2 pixels = m_Data.OriginalSize;
        const auto& texture = GetTexture();
        if (!texture)
            return { glm::vec2(0), m_Data.Pivot, m_Data.UvRect };
        if (pixels.x == 0)
            pixels.x = texture->GetWidth() * (m_Data.UvRect.z - m_Data.UvRect.x);
        if (pixels.y == 0)
            pixels.y = texture->GetHeight() * (m_Data.UvRect.w - m_Data.UvRect.y);
        return { pixels / m_Data.PixelsPerUnit, m_Data.Pivot, m_Data.UvRect };
    }
} // namespace Crowny

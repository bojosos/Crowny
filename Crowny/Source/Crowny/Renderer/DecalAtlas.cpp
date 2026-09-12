#include "cwpch.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/RenderCapabilities.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/Renderer/DecalAtlas.h"

namespace Crowny
{
    bool DecalAtlas::Update(const Vector<Ref<Texture>>& textures)
    {
        if (m_Atlas && m_Textures == textures)
            return true;
        if (!m_Copy.IsValid())
        {
            auto shader = AssetManager::Get().Load<Shader>("Resources/Shaders/DecalAtlasCopy.asset");
            auto depth = CreateRef<DepthStencilStateDesc>();
            depth->EnableDepthRead = false;
            depth->EnableDepthWrite = false;
            if (!shader || !m_Copy.Initialize(shader, nullptr, depth))
                return false;
        }
        uint32_t pageSize = 256;
        for (const auto& texture : textures)
        {
            if (!texture || texture->GetWidth() > 4094 || texture->GetHeight() > 4094)
            {
                CW_ENGINE_ERROR("Decal atlas admits textures up to 4094 pixels per side; change the texture import limit.");
                return false;
            }
            while (pageSize < std::max(texture->GetWidth(), texture->GetHeight()) + 2)
                pageSize *= 2;
        }
        struct Tile
        {
            uint32_t Texture, Mip, X, Y, Width, Height, Layer;
        };
        Vector<Tile> tiles;
        Vector<glm::vec4> rects(textures.size() * 33, glm::vec4(0));
        uint32_t x = 0, y = 0, rowHeight = 0, layer = 0;
        for (uint32_t index = 0; index < textures.size(); ++index)
        {
            const auto& texture = textures[index];
            uint32_t count = std::min(texture->GetDesc().MipLevels + 1, 16u);
            rects[index * 33] = { texture->GetWidth(), texture->GetHeight(), count, 0 };
            for (uint32_t mip = 0; mip < count; ++mip)
            {
                uint32_t w = std::max(texture->GetWidth() >> mip, 1u), h = std::max(texture->GetHeight() >> mip, 1u);
                if (x + w + 2 > pageSize)
                {
                    x = 0;
                    y += rowHeight;
                    rowHeight = 0;
                }
                if (y + h + 2 > pageSize)
                {
                    x = y = rowHeight = 0;
                    ++layer;
                }
                tiles.push_back({ index, mip, x, y, w, h, layer });
                rects[index * 33 + 1 + mip * 2] = glm::vec4(x + 1, y + 1, w, h) / static_cast<float>(pageSize);
                rects[index * 33 + 2 + mip * 2] = { layer, 0, 0, 0 };
                x += w + 2;
                rowHeight = std::max(rowHeight, h + 2);
            }
        }
        if (uint64_t(pageSize) * pageSize * std::max(layer + 1, 2u) * 8 > 256ull * 1024 * 1024)
        {
            CW_ENGINE_ERROR("Decal compatibility atlas exceeds its 256 MiB budget.");
            return false;
        }
        const auto& capabilities = RenderAPI::Get().GetCapabilities();
        if (pageSize > capabilities.MaxTexture2DSize || std::max(layer + 1, 2u) > capabilities.MaxTextureArrayLayers)
        {
            CW_ENGINE_ERROR("Decal atlas requires {} pixels and {} array layers; device limits are {} pixels and {} layers.", pageSize,
                            std::max(layer + 1, 2u), capabilities.MaxTexture2DSize, capabilities.MaxTextureArrayLayers);
            return false;
        }
        TextureDesc desc;
        desc.Width = desc.Height = pageSize;
        desc.Faces = std::max(layer + 1, 2u);
        desc.Format = TextureFormat::RGBA16F;
        desc.sRGB = false;
        desc.Usage = TextureUsage::TEXTURE_RENDERTARGET;
        desc.DebugName = "Decal texture atlas";
        auto atlas = Texture::Create(desc);
        auto& api = RenderAPI::Get();
        auto sampler = SamplerState::Create(SamplerStateDesc{});
        // The sampled array view includes every allocated layer, including the spare
        // layer needed to keep a single-page atlas a texture array.
        for (uint32_t page = 0; page < desc.Faces; ++page)
        {
            RenderTextureDesc targetDesc;
            targetDesc.Width = targetDesc.Height = pageSize;
            targetDesc.ColorSurfaces[0].Texture = atlas;
            targetDesc.ColorSurfaces[0].Face = page;
            targetDesc.ColorSurfaces[0].NumFaces = 1;
            auto target = RenderTexture::Create(targetDesc);
            api.SetRenderTarget(target);
            api.ClearRenderTarget(FBT_COLOR, glm::vec4(0.0f));
            for (const Tile& tile : tiles)
            {
                if (tile.Layer != page)
                    continue;
                glm::vec4 constants(tile.Width, tile.Height, tile.Mip, 0);
                m_Copy.SetTexture(0, 0, textures[tile.Texture]);
                m_Copy.SetSamplerState(0, 0, sampler);
                m_Copy.WriteUniformBlock(0, 1, &constants, sizeof(constants));
                api.SetViewport(float(tile.X) / pageSize, float(tile.Y) / pageSize, float(tile.Width + 2) / pageSize,
                                float(tile.Height + 2) / pageSize);
                if (!m_Copy.Bind())
                    return false;
                api.SetVertexLayout(CreateRef<BufferLayout>());
                api.Draw(0, 3, 1);
            }
        }
        GenericGpuBufferDesc bufferDesc;
        bufferDesc.ElementCount = std::max(static_cast<uint32_t>(rects.size()), 1u);
        bufferDesc.ElementSize = sizeof(glm::vec4);
        bufferDesc.Type = GpuBufferType::Structured;
        m_Rects = GenericGpuBuffer::Create(bufferDesc);
        if (!rects.empty())
            m_Rects->WriteData(0, static_cast<uint32_t>(rects.size() * sizeof(glm::vec4)), rects.data());
        m_Atlas = atlas;
        m_Textures = textures;
        return true;
    }
} // namespace Crowny

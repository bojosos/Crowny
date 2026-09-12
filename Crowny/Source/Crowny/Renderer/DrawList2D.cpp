#include "cwpch.h"

#include "Crowny/Renderer/DrawList2D.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace Crowny
{
    DrawList2D::DrawList2D(uint32_t textureCapacity, uint32_t instanceCapacity)
      : m_TextureCapacity(std::clamp(textureCapacity, 1u, 8u)), m_InstanceCapacity(std::max(instanceCapacity, 1u))
    {
    }

    void DrawList2D::Reserve(size_t instances, size_t batches)
    {
        m_Instances.reserve(instances);
        m_Batches.reserve(batches);
    }

    void DrawList2D::Clear()
    {
        m_Instances.clear();
        m_Batches.clear();
    }

    void DrawList2D::Append(const DrawItem2D& item)
    {
        if (m_Instances.size() == std::numeric_limits<uint32_t>::max())
            throw std::length_error("2D draw list exceeds the 32-bit instance range");

        BatchBreak2D reason = BatchBreak2D::First;
        uint32_t slot = 0;
        if (!m_Batches.empty())
        {
            DrawBatch2D& batch = m_Batches.back();
            while (slot < batch.TextureCount && batch.Textures[slot] != item.Texture)
                ++slot;
            if (batch.Primitive != item.Primitive || batch.Material != item.Material || batch.Clip != item.Clip)
                reason = BatchBreak2D::State;
            else if (batch.StoragePage != item.StoragePage)
                reason = BatchBreak2D::StoragePage;
            else if (batch.InstanceCount == m_InstanceCapacity)
                reason = BatchBreak2D::InstanceCapacity;
            else if (slot == m_TextureCapacity)
                reason = BatchBreak2D::TextureCapacity;
            else
            {
                if (slot == batch.TextureCount)
                    batch.Textures[batch.TextureCount++] = item.Texture;
                m_Instances.push_back({ item.Instance, slot });
                ++batch.InstanceCount;
                return;
            }
        }

        DrawBatch2D batch;
        batch.FirstInstance = static_cast<uint32_t>(m_Instances.size());
        batch.InstanceCount = 1;
        batch.Material = item.Material;
        batch.Clip = item.Clip;
        batch.Primitive = item.Primitive;
        batch.StoragePage = item.StoragePage;
        batch.Break = reason;
        batch.TextureCount = 1;
        batch.Textures[0] = item.Texture;
        m_Batches.push_back(batch);
        m_Instances.push_back({ item.Instance, 0 });
    }

    void DrawList2D::Append(std::span<const DrawItem2D> items)
    {
        for (const DrawItem2D& item : items)
            Append(item);
    }
} // namespace Crowny

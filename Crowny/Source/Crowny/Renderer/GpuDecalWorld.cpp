#include "cwpch.h"

#include "Crowny/Renderer/GpuDecalWorld.h"

#include "Crowny/RenderAPI/RenderCapabilities.h"
#include "Crowny/Renderer/GpuScene.h"

#include <bit>

namespace Crowny
{
    bool GpuDecalWorld::IsAlive(DecalHandle handle) const
    {
        return handle.Index < m_Slots.size() && m_Slots[handle.Index].Alive && m_Slots[handle.Index].Generation == handle.Generation;
    }

    bool GpuDecalWorld::TryGet(DecalHandle handle, RenderableDecal& record) const
    {
        if (!IsAlive(handle))
            return false;
        record = m_Slots[handle.Index].Source;
        return true;
    }

    bool GpuDecalWorld::GetReference(DecalHandle handle, glm::uvec2& reference) const
    {
        if (!IsAlive(handle))
            return false;
        const auto material = m_Slots[handle.Index].Material;
        if (material.x >= m_MaterialSlots.size() || !m_MaterialSlots[material.x].Resident || m_MaterialSlots[material.x].Generation != material.y)
            return false;
        reference = { handle.Index, handle.Generation };
        return true;
    }

    glm::uvec2 GpuDecalWorld::GetMaterialReference(DecalHandle handle) const
    {
        return IsAlive(handle) ? m_Slots[handle.Index].Material : glm::uvec2(UINT32_MAX, 0);
    }

    glm::uvec2 GpuDecalWorld::AcquireMaterial(const RenderableDecal& source)
    {
        uint32_t index;
        const auto existing = m_MaterialIndices.find(source.MaterialId);
        if (existing != m_MaterialIndices.end())
            index = existing->second;
        else
        {
            if (m_FreeMaterials.empty())
            {
                index = static_cast<uint32_t>(m_MaterialSlots.size());
                m_MaterialSlots.emplace_back();
            }
            else
            {
                index = m_FreeMaterials.back();
                m_FreeMaterials.pop_back();
            }
            m_MaterialIndices[source.MaterialId] = index;
        }
        auto& material = m_MaterialSlots[index];
        material.Id = source.MaterialId;
        material.Source = source.Material;
        material.Textures = source.Textures;
        ++material.References;
        return { index, material.Generation };
    }

    void GpuDecalWorld::ReleaseMaterial(glm::uvec2 handle)
    {
        auto& material = m_MaterialSlots[handle.x];
        if (material.Generation != handle.y || material.References == 0 || --material.References != 0)
            return;
        m_MaterialIndices.erase(material.Id);
        material.Textures = {};
        material.Resident = false;
        if (++material.Generation == 0)
            ++material.Generation;
        m_FreeMaterials.push_back(handle.x);
    }

    void GpuDecalWorld::Apply(std::span<const RenderableDecalChange> changes)
    {
        for (const auto& change : changes)
        {
            if (change.Handle.Index == UINT32_MAX || change.Handle.Generation == 0)
                continue;
            if (change.Type == DecalChangeType::Create)
            {
                if (change.Handle.Index >= m_Slots.size())
                    m_Slots.resize(size_t(change.Handle.Index) + 1);
                auto& slot = m_Slots[change.Handle.Index];
                if (slot.Alive || (slot.Generation != 0 && static_cast<int32_t>(change.Handle.Generation - slot.Generation) <= 0))
                    continue;
                slot.Generation = change.Handle.Generation;
                slot.Alive = true;
                slot.Source = change.Record;
                slot.Source.Handle = change.Handle;
                slot.Material = AcquireMaterial(change.Record);
                ++m_ActiveCount;
            }
            else if (IsAlive(change.Handle))
            {
                auto& slot = m_Slots[change.Handle.Index];
                if (change.Type == DecalChangeType::Destroy)
                {
                    ReleaseMaterial(slot.Material);
                    slot.Source = {};
                    slot.Alive = false;
                    --m_ActiveCount;
                }
                else
                {
                    if (slot.Source.MaterialId != change.Record.MaterialId)
                    {
                        ReleaseMaterial(slot.Material);
                        slot.Material = AcquireMaterial(change.Record);
                    }
                    else
                    {
                        auto& material = m_MaterialSlots[slot.Material.x];
                        material.Source = change.Record.Material;
                        material.Textures = change.Record.Textures;
                    }
                    slot.Source = change.Record;
                    slot.Source.Handle = change.Handle;
                }
            }
        }
    }

    bool GpuDecalWorld::Upload(Ref<GenericGpuBuffer>& buffer, Vector<uint8_t>& previous, const void* data, uint32_t count, uint32_t stride,
                               DecalRenderStats& stats)
    {
        const uint64_t bytes = uint64_t(std::max(count, 1u)) * stride;
        const auto& capabilities = RenderAPI::Get().GetCapabilities();
        if (bytes > UINT32_MAX || bytes > capabilities.MaxStorageBufferRange)
            return false;
        const std::array<uint8_t, sizeof(GpuDecalSlot)> empty{};
        const auto* source = count ? static_cast<const uint8_t*>(data) : empty.data();
        if (!buffer || buffer->GetBufferSize() < bytes)
        {
            const uint32_t capacity = std::bit_ceil(std::max(count, 1u));
            const uint32_t admittedCapacity =
              uint64_t(capacity) * stride <= std::min<uint64_t>(capabilities.MaxStorageBufferRange, UINT32_MAX) ? capacity : std::max(count, 1u);
            buffer = GenericGpuBuffer::Create({ admittedCapacity, stride, GpuBufferType::Structured, BF_UNKNOWN, BufferUsage::BU_DYNAMIC_DRAW });
            previous.clear();
        }
        if (!buffer)
            return false;
        if (previous.size() != bytes)
        {
            buffer->WriteData(0, static_cast<uint32_t>(bytes), source, BWT_DISCARD);
            stats.UploadedBytes += bytes;
        }
        else
        {
            for (uint32_t first = 0; first < std::max(count, 1u);)
            {
                if (std::memcmp(previous.data() + first * stride, source + first * stride, stride) == 0)
                {
                    ++first;
                    continue;
                }
                uint32_t end = first + 1;
                while (end < count && std::memcmp(previous.data() + end * stride, source + end * stride, stride) != 0)
                    ++end;
                const uint32_t size = (end - first) * stride;
                buffer->WriteData(first * stride, size, source + first * stride, BWT_NORMAL);
                stats.UploadedBytes += size;
                first = end;
            }
        }
        previous.assign(source, source + bytes);
        return true;
    }

    bool GpuDecalWorld::Prepare(GpuScene* scene, DecalRenderStats& stats)
    {
        m_Textures = { Texture::WHITE };
        UnorderedMap<const Texture*, uint32_t> textureIndices{ { Texture::WHITE.get(), 0 } };
        m_MaterialData.assign(m_MaterialSlots.size(), {});
        for (uint32_t index = 0; index < m_MaterialSlots.size(); ++index)
        {
            auto& material = m_MaterialSlots[index];
            if (material.References == 0)
                continue;
            material.Resident = true;
            const size_t before = m_Textures.size();
            uint32_t indices[5]{};
            for (uint32_t channel = 0; channel < 5; ++channel)
            {
                auto texture = material.Textures[channel] ? material.Textures[channel] : channel == 1 ? Texture::NORMAL : Texture::WHITE;
                auto found = textureIndices.find(texture.get());
                if (found != textureIndices.end())
                    indices[channel] = found->second;
                else if (!scene && m_Textures.size() >= 256)
                {
                    material.Resident = false;
                    break;
                }
                else
                {
                    indices[channel] = static_cast<uint32_t>(m_Textures.size());
                    textureIndices[texture.get()] = indices[channel];
                    m_Textures.push_back(texture);
                }
            }
            if (!material.Resident)
            {
                while (m_Textures.size() > before)
                {
                    textureIndices.erase(m_Textures.back().get());
                    m_Textures.pop_back();
                }
                ++stats.RejectedMaterials;
                continue;
            }
            auto& record = m_MaterialData[index];
            record.Data = material.Source;
            record.Data.Textures = { indices[0], indices[1], indices[2], indices[3] };
            record.Data.Textures2.x = indices[4];
            record.Generation.x = material.Generation;
        }
        if (scene)
        {
            Vector<uint32_t> descriptors;
            scene->SetDecalTextures(m_Textures, descriptors);
            for (uint32_t index = 0; index < m_MaterialData.size(); ++index)
            {
                auto& record = m_MaterialData[index];
                if (record.Generation.x == 0)
                    continue;
                bool resident = true;
                for (uint32_t channel = 0; channel < 4; ++channel)
                {
                    record.Data.Textures[channel] = descriptors[record.Data.Textures[channel]];
                    resident &= record.Data.Textures[channel] != UINT32_MAX;
                }
                record.Data.Textures2.x = descriptors[record.Data.Textures2.x];
                resident &= record.Data.Textures2.x != UINT32_MAX;
                if (!resident)
                {
                    record.Generation.x = 0;
                    ++stats.RejectedMaterials;
                }
                m_MaterialSlots[index].Resident = resident;
            }
        }
        m_DecalData.assign(m_Slots.size(), {});
        for (uint32_t index = 0; index < m_Slots.size(); ++index)
        {
            const auto& slot = m_Slots[index];
            if (!slot.Alive)
                continue;
            m_DecalData[index] = { slot.Source.Data, { slot.Generation, slot.Material.y, 0, 0 } };
            m_DecalData[index].Data.Metadata.w = slot.Material.x;
        }
        stats.TextureCount = static_cast<uint32_t>(m_Textures.size());
        return Upload(m_Decals, m_PreviousDecals, m_DecalData.data(), static_cast<uint32_t>(m_DecalData.size()), sizeof(GpuDecalSlot), stats) &&
               Upload(m_Materials, m_PreviousMaterials, m_MaterialData.data(), static_cast<uint32_t>(m_MaterialData.size()),
                      sizeof(GpuDecalMaterialSlot), stats);
    }
} // namespace Crowny

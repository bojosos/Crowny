#include "cwpch.h"

#include "Crowny/Renderer/GpuWorld2D.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/RenderAPI/GenericGpuBuffer.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/RenderCapabilities.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/RenderSnapshot.h"
#include "Crowny/Renderer/TextLayoutCache.h"
#include "Crowny/Renderer/VisibilityCulling.h"

#include <algorithm>
#include <bit>
#include <chrono>

namespace Crowny
{
    GpuWorld2D::GpuWorld2D(uint32_t maxInstancesPerPage, uint32_t maxOrderEntriesPerPage)
      : m_MaxInstancesPerPage(std::clamp(maxInstancesPerPage, 1u, RenderHandle2D::MaxInstances)),
        m_MaxOrderEntriesPerPage(std::clamp(maxOrderEntriesPerPage, 1u, RenderHandle2D::MaxInstances))
    {
    }

    uint32_t GpuWorld2D::AcquireTexture(const Ref<Texture>& texture)
    {
        if (!texture || texture == Texture::WHITE)
            return 0;
        const auto found = m_TextureLookup.find(texture.get());
        if (found != m_TextureLookup.end())
        {
            ++m_Textures[found->second].Users;
            return found->second;
        }
        uint32_t index;
        if (m_FreeTextures.empty())
        {
            index = static_cast<uint32_t>(m_Textures.size());
            m_Textures.emplace_back();
        }
        else
        {
            index = m_FreeTextures.back();
            m_FreeTextures.pop_back();
        }
        m_Textures[index] = { texture, 1 };
        m_TextureLookup.emplace(texture.get(), index);
        return index;
    }

    void GpuWorld2D::ReleaseTexture(uint32_t index)
    {
        if (index == 0)
            return;
        TextureEntry& entry = m_Textures[index];
        if (--entry.Users == 0)
        {
            m_TextureLookup.erase(entry.Resource.get());
            entry.Resource.Reset();
            m_FreeTextures.push_back(index);
        }
    }

    void GpuWorld2D::Apply(std::span<const RenderChange2D> changes)
    {
        for (const RenderChange2D& change : changes)
        {
            if (!change.Handle || change.Type == RenderChange2DType::Cancelled)
                continue;
            const uint32_t index = change.Handle.GetIndex();
            if (index >= m_Slots.size())
            {
                if (change.Type != RenderChange2DType::Create)
                    continue;
                m_Slots.resize(index + 1);
                m_Instances.resize(index + 1);
            }
            Slot& slot = m_Slots[index];
            if (change.Type != RenderChange2DType::Create && slot.Handle != change.Handle)
                continue;
            if (change.Type == RenderChange2DType::Destroy)
            {
                ReleaseTexture(slot.Texture);
                slot = {};
                continue;
            }
            const Ref<Texture> previous = slot.Texture ? m_Textures[slot.Texture].Resource : nullptr;
            if (previous != change.TextureResource)
            {
                const uint32_t texture = AcquireTexture(change.TextureResource);
                ReleaseTexture(slot.Texture);
                slot.Texture = texture;
            }
            slot.Handle = change.Handle;
            slot.Visible = change.Visible;
            const Instance updated{ change.Data, { change.ObjectID.Value, change.VisibilityLayers.Value, 0, 0 } };
            // Ordering and texture-table changes do not change this GPU record.
            // The packed record consists entirely of vec4 rows with no padding.
            if (change.Type == RenderChange2DType::Create || std::memcmp(&m_Instances[index], &updated, sizeof(Instance)) != 0)
            {
                m_Instances[index] = updated;
                m_Dirty.push_back(index);
            }
        }
    }

    bool GpuWorld2D::Upload()
    {
        if (m_Instances.empty())
            return true;
        const uint64_t limit = RenderAPI::TryGet()->GetCapabilities().MaxStorageBufferRange;
        if (m_InstancesPerPage == 0)
        {
            const uint64_t available = limit ? limit / sizeof(Instance) : m_MaxInstancesPerPage;
            m_InstancesPerPage = std::bit_floor(static_cast<uint32_t>(std::min<uint64_t>(available, m_MaxInstancesPerPage)));
            if (m_InstancesPerPage == 0)
                return false;
        }
        const uint32_t pageCount = static_cast<uint32_t>((m_Instances.size() + m_InstancesPerPage - 1) / m_InstancesPerPage);
        m_Pages.resize(pageCount);
        std::sort(m_Dirty.begin(), m_Dirty.end());
        size_t dirty = 0;
        for (uint32_t pageIndex = 0; pageIndex < pageCount; ++pageIndex)
        {
            Page& page = m_Pages[pageIndex];
            const uint32_t first = pageIndex * m_InstancesPerPage;
            const uint32_t count = std::min(m_InstancesPerPage, static_cast<uint32_t>(m_Instances.size()) - first);
            const auto write = [&](uint32_t begin, uint32_t end, BufferWriteOptions writeType) {
                const uint32_t bytes = (end - begin) * static_cast<uint32_t>(sizeof(Instance));
                page.Buffer->WriteData((begin - first) * sizeof(Instance), bytes, m_Instances.data() + begin, writeType);
                m_Statistics.UploadedBytes += bytes;
                m_Statistics.InstanceUploadBytes += bytes;
            };
            if (!page.Buffer)
            {
                page.Buffer = GenericGpuBuffer::Create(
                  { m_InstancesPerPage, sizeof(Instance), GpuBufferType::Structured, BF_UNKNOWN, BufferUsage::BU_DYNAMIC_DRAW });
                if (!page.Buffer)
                    return false;
                write(first, first + count, BWT_DISCARD);
                while (dirty < m_Dirty.size() && m_Dirty[dirty] < first + count)
                    ++dirty;
                continue;
            }
            while (dirty < m_Dirty.size() && m_Dirty[dirty] < first + count)
            {
                const uint32_t begin = m_Dirty[dirty++];
                uint32_t end = begin + 1;
                while (dirty < m_Dirty.size() && m_Dirty[dirty] <= end && m_Dirty[dirty] < first + count)
                    end = std::max(end, m_Dirty[dirty++] + 1);
                write(begin, end, BWT_NORMAL);
            }
        }
        m_Dirty.clear();
        return true;
    }

    void GpuWorld2D::ReleaseView(uint64_t viewIdentity) { m_Views.erase(viewIdentity); }

    bool GpuWorld2D::Prepare(const RenderSnapshot& snapshot, View& view)
    {
        view.DrawList.Clear();
        view.DrawList.Reserve(snapshot.Ordered2D.Size(), snapshot.Ordered2D.Size() / 256 + 1);
        const VisibilityFrustum frustum = VisibilityFrustum::FromViewProjection(snapshot.ProjectionMatrix * snapshot.ViewMatrix);
        for (const Renderable2DOrder& item : snapshot.Ordered2D)
        {
            if (item.Type == Renderable2DType::Text)
            {
                if (item.Index >= snapshot.Texts.Size())
                    return false;
                const auto& text = snapshot.Texts[item.Index];
                if (!m_Text.Prepare(text.Layout))
                    return false;
                if (text.Layout)
                    m_Statistics.Glyphs += static_cast<uint32_t>(text.Layout->View().GlyphCount);
                view.DrawList.Append({ item.Index, 0, 0, 0, Primitive2D::Glyph });
                continue;
            }
            if (item.Index >= snapshot.Sprites.Size())
                return false;
            const RenderableSprite& sprite = snapshot.Sprites[item.Index];
            ++m_Statistics.Submitted;
            if (!sprite.Handle || sprite.Handle.GetIndex() >= m_Slots.size())
                return false;
            const Slot& slot = m_Slots[sprite.Handle.GetIndex()];
            if (slot.Handle != sprite.Handle)
                return false;
            if (!slot.Visible)
                continue;
            const float radius = 0.5f * (glm::length(glm::vec3(sprite.WorldMatrix[0])) + glm::length(glm::vec3(sprite.WorldMatrix[1])));
            if (!frustum.IntersectsSphere(glm::vec3(sprite.WorldMatrix[3]), radius))
                continue;
            ++m_Statistics.Visible;
            const uint32_t index = sprite.Handle.GetIndex();
            view.DrawList.Append({ index % m_InstancesPerPage, slot.Texture, 0, 0, Primitive2D::Sprite, index / m_InstancesPerPage });
        }
        for (const DrawBatch2D& batch : view.DrawList.GetBatches())
            ++m_Statistics.BatchBreaks[static_cast<size_t>(batch.Break)];
        const auto order = view.DrawList.GetInstances();
        // Text draws read their retained glyph pages directly; they do not
        // consume the sprite draw-order buffer.
        if (order.empty() || m_Statistics.Visible == 0)
            return true;
        if (m_OrderEntriesPerPage == 0)
        {
            const uint64_t limit = RenderAPI::TryGet()->GetCapabilities().MaxStorageBufferRange;
            const uint64_t available = limit ? limit / sizeof(DrawInstance2D) : m_MaxOrderEntriesPerPage;
            m_OrderEntriesPerPage = static_cast<uint32_t>(std::min<uint64_t>(available, m_MaxOrderEntriesPerPage));
            if (m_OrderEntriesPerPage == 0)
                return false;
        }
        const size_t pageCount = (order.size() + m_OrderEntriesPerPage - 1) / m_OrderEntriesPerPage;
        // Keep unused pages resident so temporary visibility changes do not
        // allocate buffers or invalidate their contents when objects return.
        if (view.OrderPages.size() < pageCount)
            view.OrderPages.resize(pageCount);
        bool changed = false;
        for (size_t pageIndex = 0; pageIndex < pageCount; ++pageIndex)
        {
            OrderPage& page = view.OrderPages[pageIndex];
            const size_t first = pageIndex * m_OrderEntriesPerPage;
            const auto entries = order.subspan(first, std::min<size_t>(m_OrderEntriesPerPage, order.size() - first));
            if (!page.Buffer)
                page.Buffer = GenericGpuBuffer::Create(
                  { m_OrderEntriesPerPage, sizeof(DrawInstance2D), GpuBufferType::Structured, BF_UNKNOWN, BufferUsage::BU_DYNAMIC_DRAW });
            if (!page.Buffer)
                return false;
            if (entries.size() == page.PreviousOrder.size() && std::equal(entries.begin(), entries.end(), page.PreviousOrder.begin()))
                continue;
            const uint32_t bytes = static_cast<uint32_t>(entries.size_bytes());
            page.Buffer->WriteData(0, bytes, entries.data(), BWT_DISCARD);
            m_Statistics.UploadedBytes += bytes;
            m_Statistics.OrderUploadBytes += bytes;
            page.PreviousOrder.assign(entries.begin(), entries.end());
            changed = true;
        }
        if (changed)
            ++m_Statistics.OrderCacheMisses;
        return true;
    }

    bool GpuWorld2D::Render(const RenderSnapshot& snapshot)
    {
        m_Statistics = {};
        m_Text.BeginView();
        View& view = m_Views[snapshot.HistoryNamespace];
        const auto start = std::chrono::steady_clock::now();
        if (!Upload())
            return false;
        const auto uploaded = std::chrono::steady_clock::now();
        m_Statistics.UploadCpuTimeMs = std::chrono::duration<double, std::milli>(uploaded - start).count();
        if (!Prepare(snapshot, view))
            return false;
        const auto prepared = std::chrono::steady_clock::now();
        m_Statistics.PrepareCpuTimeMs = std::chrono::duration<double, std::milli>(prepared - uploaded).count();
        if (view.DrawList.GetBatches().empty())
            return true;
        const auto& batches = view.DrawList.GetBatches();
        const bool hasSprites =
          std::any_of(batches.begin(), batches.end(), [](const DrawBatch2D& batch) { return batch.Primitive != Primitive2D::Glyph; });
        if (hasSprites && !m_Material.IsValid())
        {
            const auto shader = AssetManager::TryGet()->Load<Shader>("Resources/Shaders/Sprite2D.asset");
            const Ref<DepthStencilStateDesc> depth = CreateRef<DepthStencilStateDesc>();
            depth->EnableDepthWrite = false;
            depth->DepthCompareFunction = RenderAPI::TryGet()->GetCapabilities().GetFeatureTier() == RenderFeatureTier::Compatibility
                                            ? CompareFunction::LESS_EQUAL
                                            : CompareFunction::GREATER_EQUAL;
            if (!m_Material.Initialize(shader, nullptr, depth))
                return false;
            m_EmptyLayout = CreateRef<BufferLayout>();
        }
        struct alignas(16) Constants
        {
            glm::mat4 ViewProjection;
            glm::uvec4 Draw;
        } constants{ snapshot.ProjectionMatrix * snapshot.ViewMatrix, {} };

        for (const DrawBatch2D& batch : view.DrawList.GetBatches())
        {
            if (batch.Primitive == Primitive2D::Glyph)
            {
                for (uint32_t i = 0; i < batch.InstanceCount; ++i)
                {
                    const uint32_t textIndex = view.DrawList.GetInstances()[batch.FirstInstance + i].Instance;
                    if (textIndex < snapshot.Texts.Size())
                    {
                        const auto& text = snapshot.Texts[textIndex];
                        if (!m_Text.Render(text, constants.ViewProjection))
                            return false;
                    }
                }
                continue;
            }
            if (batch.StoragePage >= m_Pages.size() || !m_Material.SetBuffer(0, 9, m_Pages[batch.StoragePage].Buffer))
                return false;
            for (uint32_t i = 0; i < 8; ++i)
            {
                const uint32_t texture = i < batch.TextureCount ? batch.Textures[i] : 0;
                m_Material.SetTexture(0, i + 1, texture ? m_Textures[texture].Resource : Texture::WHITE);
            }
            uint32_t first = batch.FirstInstance;
            uint32_t remaining = batch.InstanceCount;
            while (remaining)
            {
                const uint32_t pageIndex = first / m_OrderEntriesPerPage;
                constants.Draw.x = first % m_OrderEntriesPerPage;
                const uint32_t count = std::min(remaining, m_OrderEntriesPerPage - constants.Draw.x);
                if (!m_Material.SetBuffer(0, 10, view.OrderPages[pageIndex].Buffer))
                    return false;
                m_Material.WriteUniformBlock(0, 0, &constants, sizeof(constants));
                m_Statistics.UploadedBytes += sizeof(constants);
                m_Statistics.UniformUploadBytes += sizeof(constants);
                m_Material.Bind();
                RenderAPI::TryGet()->SetVertexLayout(m_EmptyLayout);
                RenderAPI::TryGet()->SetDrawMode(DrawMode::TRIANGLE_LIST);
                RenderAPI::TryGet()->Draw(0, 6, count);
                ++m_Statistics.Batches;
                if (first != batch.FirstInstance)
                    ++m_Statistics.BatchBreaks[static_cast<size_t>(BatchBreak2D::OrderPage)];
                first += count;
                remaining -= count;
            }
        }
        const auto& textStatistics = m_Text.GetStatistics();
        m_Statistics.GlyphUploadBytes = textStatistics.GeometryUploadBytes;
        m_Statistics.TextGeometryBuilds = textStatistics.GeometryBuilds;
        m_Statistics.CulledTextObjects = textStatistics.CulledObjects;
        m_Statistics.UniformUploadBytes += textStatistics.UniformUploadBytes;
        m_Statistics.UploadedBytes += textStatistics.GeometryUploadBytes + textStatistics.UniformUploadBytes;
        m_Statistics.Batches += textStatistics.Batches;
        m_Statistics.SubmissionCpuTimeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - prepared).count();
        return true;
    }
} // namespace Crowny

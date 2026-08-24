#include "cwpch.h"

#include "Crowny/Renderer/GpuBufferPool.h"

namespace Crowny
{
    namespace
    {
        void HashCombine(size_t& hash, uint32_t value)
        {
            hash ^= static_cast<size_t>(value) + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
        }
    } // namespace

    GpuBufferPool::GpuBufferPool(uint32_t framesInFlight, uint64_t retainedByteBudget)
      : m_FramesInFlight(std::max(framesInFlight, 1u)), m_RetainedByteBudget(retainedByteBudget)
    {
    }

    void GpuBufferPool::BeginFrame(uint64_t frameNumber)
    {
        std::scoped_lock lock(m_Mutex);
        m_CurrentFrame = frameNumber;
        MakeReady();
    }

    Ref<GenericGpuBuffer> GpuBufferPool::Acquire(const GenericGpuBufferDesc& desc)
    {
        Ref<GpuBuffer> pooled = Acquire(MakeKey(desc));
        if (pooled)
            return pooled.StaticCast<GenericGpuBuffer>();

        Ref<GenericGpuBuffer> created = GenericGpuBuffer::Create(desc);
        if (created)
        {
            std::scoped_lock lock(m_Mutex);
            m_Stats.Created++;
        }
        return created;
    }

    Ref<VertexBuffer> GpuBufferPool::Acquire(const VertexBufferDesc& desc)
    {
        Ref<GpuBuffer> pooled = Acquire(MakeKey(desc));
        Ref<VertexBuffer> buffer;
        if (pooled)
        {
            buffer = pooled.StaticCast<VertexBuffer>();
            buffer->SetLayout(nullptr);
            if (desc.Data != nullptr && desc.Size != 0)
                buffer->WriteData(0, desc.Size, desc.Data, BWT_DISCARD);
            return buffer;
        }

        buffer = VertexBuffer::Create(desc);
        if (buffer)
        {
            std::scoped_lock lock(m_Mutex);
            m_Stats.Created++;
        }
        return buffer;
    }

    Ref<IndexBuffer> GpuBufferPool::Acquire(const IndexBufferDesc& desc)
    {
        Ref<GpuBuffer> pooled = Acquire(MakeKey(desc));
        Ref<IndexBuffer> buffer;
        if (pooled)
        {
            buffer = pooled.StaticCast<IndexBuffer>();
            const uint32_t size = desc.Count * IndexElementSize(desc.Type);
            if (desc.Data != nullptr && size != 0)
                buffer->WriteData(0, size, desc.Data, BWT_DISCARD);
            return buffer;
        }

        buffer = IndexBuffer::Create(desc);
        if (buffer)
        {
            std::scoped_lock lock(m_Mutex);
            m_Stats.Created++;
        }
        return buffer;
    }

    void GpuBufferPool::Release(const GenericGpuBufferDesc& desc, Ref<GenericGpuBuffer>&& buffer)
    {
        Ref<GpuBuffer> base = std::move(buffer);
        Release(MakeKey(desc), std::move(base));
    }

    void GpuBufferPool::Release(const VertexBufferDesc& desc, Ref<VertexBuffer>&& buffer)
    {
        Ref<GpuBuffer> base = std::move(buffer);
        Release(MakeKey(desc), std::move(base));
    }

    void GpuBufferPool::Release(const IndexBufferDesc& desc, Ref<IndexBuffer>&& buffer)
    {
        Ref<GpuBuffer> base = std::move(buffer);
        Release(MakeKey(desc), std::move(base));
    }

    void GpuBufferPool::SetRetainedByteBudget(uint64_t byteBudget)
    {
        std::scoped_lock lock(m_Mutex);
        m_RetainedByteBudget = byteBudget;
        TrimLocked();
    }

    void GpuBufferPool::Trim()
    {
        std::scoped_lock lock(m_Mutex);
        m_Available.clear();
        m_Retired.clear();
        m_Stats.RetainedBytes = 0;
        m_Stats.AvailableBuffers = 0;
        m_Stats.RetiredBuffers = 0;
    }

    GpuBufferPoolStats GpuBufferPool::GetStats() const
    {
        std::scoped_lock lock(m_Mutex);
        return m_Stats;
    }

    size_t GpuBufferPool::BufferKeyHash::operator()(const BufferKey& key) const
    {
        size_t hash = static_cast<size_t>(key.Kind);
        HashCombine(hash, key.Size);
        HashCombine(hash, key.ElementSize);
        HashCombine(hash, key.TypeOrIndex);
        HashCombine(hash, key.Format);
        HashCombine(hash, key.Usage);
        return hash;
    }

    GpuBufferPool::BufferKey GpuBufferPool::MakeKey(const GenericGpuBufferDesc& desc)
    {
        return { BufferKind::Generic, desc.ElementCount * desc.ElementSize, desc.ElementSize,
                 static_cast<uint32_t>(desc.Type), static_cast<uint32_t>(desc.Format), static_cast<uint32_t>(desc.Usage) };
    }

    GpuBufferPool::BufferKey GpuBufferPool::MakeKey(const VertexBufferDesc& desc)
    {
        return { BufferKind::Vertex, desc.Size, 0u, 0u, 0u, static_cast<uint32_t>(desc.Usage) };
    }

    GpuBufferPool::BufferKey GpuBufferPool::MakeKey(const IndexBufferDesc& desc)
    {
        const uint32_t elementSize = IndexElementSize(desc.Type);
        return { BufferKind::Index, desc.Count * elementSize, elementSize, static_cast<uint32_t>(desc.Type), 0u,
                 static_cast<uint32_t>(desc.Usage) };
    }

    uint32_t GpuBufferPool::IndexElementSize(IndexType type)
    {
        return type == IndexType::Index_16 ? sizeof(uint16_t) : sizeof(uint32_t);
    }

    Ref<GpuBuffer> GpuBufferPool::Acquire(const BufferKey& key)
    {
        std::scoped_lock lock(m_Mutex);
        MakeReady();
        const auto found = m_Available.find(key);
        if (found == m_Available.end() || found->second.empty())
            return nullptr;

        Ref<GpuBuffer> result = std::move(found->second.back());
        found->second.pop_back();
        if (found->second.empty())
            m_Available.erase(found);
        m_Stats.Reused++;
        m_Stats.RetainedBytes -= key.Size;
        m_Stats.AvailableBuffers--;
        return result;
    }

    void GpuBufferPool::Release(const BufferKey& key, Ref<GpuBuffer>&& buffer)
    {
        if (!buffer)
            return;

        std::scoped_lock lock(m_Mutex);
        if (buffer->GetRefCount() != 1 || key.Size == 0 || key.Size > m_RetainedByteBudget ||
            m_Stats.RetainedBytes + key.Size > m_RetainedByteBudget)
        {
            m_Stats.Rejected++;
            return;
        }

        m_Retired.push_back({ key, std::move(buffer), m_CurrentFrame });
        m_Stats.RetainedBytes += key.Size;
        m_Stats.RetiredBuffers++;
    }

    void GpuBufferPool::MakeReady()
    {
        for (size_t index = 0; index < m_Retired.size();)
        {
            Entry& entry = m_Retired[index];
            if (m_CurrentFrame < entry.RetiredFrame || m_CurrentFrame - entry.RetiredFrame < m_FramesInFlight)
            {
                index++;
                continue;
            }

            m_Available[entry.Key].push_back(std::move(entry.Buffer));
            m_Retired[index] = std::move(m_Retired.back());
            m_Retired.pop_back();
            m_Stats.RetiredBuffers--;
            m_Stats.AvailableBuffers++;
        }
    }

    void GpuBufferPool::TrimLocked()
    {
        for (auto available = m_Available.begin();
             available != m_Available.end() && m_Stats.RetainedBytes > m_RetainedByteBudget;)
        {
            Vector<Ref<GpuBuffer>>& buffers = available->second;
            while (!buffers.empty() && m_Stats.RetainedBytes > m_RetainedByteBudget)
            {
                buffers.pop_back();
                m_Stats.RetainedBytes -= available->first.Size;
                m_Stats.AvailableBuffers--;
            }
            available = buffers.empty() ? m_Available.erase(available) : std::next(available);
        }

        while (!m_Retired.empty() && m_Stats.RetainedBytes > m_RetainedByteBudget)
        {
            m_Stats.RetainedBytes -= m_Retired.back().Key.Size;
            m_Retired.pop_back();
            m_Stats.RetiredBuffers--;
        }
    }
} // namespace Crowny

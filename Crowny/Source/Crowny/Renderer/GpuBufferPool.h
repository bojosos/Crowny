#pragma once

#include "Crowny/RenderAPI/GenericGpuBuffer.h"
#include "Crowny/RenderAPI/IndexBuffer.h"
#include "Crowny/RenderAPI/VertexBuffer.h"

#include <mutex>

namespace Crowny
{
    struct GpuBufferPoolStats
    {
        uint64_t Created = 0;
        uint64_t Reused = 0;
        uint64_t Rejected = 0;
        uint64_t RetainedBytes = 0;
        uint32_t AvailableBuffers = 0;
        uint32_t RetiredBuffers = 0;
    };

    // Reuses whole buffer objects whose descriptors match exactly. This sits above
    // backend memory allocators such as VMA; it avoids repeated API object creation
    // and is deliberately separate from geometry/upload suballocation.
    class GpuBufferPool
    {
    public:
        explicit GpuBufferPool(uint32_t framesInFlight = 2, uint64_t retainedByteBudget = 64ull * 1024ull * 1024ull);

        void BeginFrame(uint64_t frameNumber);

        Ref<GenericGpuBuffer> Acquire(const GenericGpuBufferDesc& desc);
        Ref<VertexBuffer> Acquire(const VertexBufferDesc& desc);
        Ref<IndexBuffer> Acquire(const IndexBufferDesc& desc);

        void Release(const GenericGpuBufferDesc& desc, Ref<GenericGpuBuffer>&& buffer);
        void Release(const VertexBufferDesc& desc, Ref<VertexBuffer>&& buffer);
        void Release(const IndexBufferDesc& desc, Ref<IndexBuffer>&& buffer);

        void SetRetainedByteBudget(uint64_t byteBudget);
        void Trim();
        GpuBufferPoolStats GetStats() const;

    private:
        enum class BufferKind : uint8_t
        {
            Generic,
            Vertex,
            Index
        };

        struct BufferKey
        {
            BufferKind Kind = BufferKind::Generic;
            uint32_t Size = 0;
            uint32_t ElementSize = 0;
            uint32_t TypeOrIndex = 0;
            uint32_t Format = 0;
            uint32_t Usage = 0;

            bool operator==(const BufferKey& other) const = default;
        };

        struct BufferKeyHash
        {
            size_t operator()(const BufferKey& key) const;
        };

        struct Entry
        {
            BufferKey Key;
            Ref<GpuBuffer> Buffer;
            uint64_t RetiredFrame = 0;
        };

        static BufferKey MakeKey(const GenericGpuBufferDesc& desc);
        static BufferKey MakeKey(const VertexBufferDesc& desc);
        static BufferKey MakeKey(const IndexBufferDesc& desc);
        static uint32_t IndexElementSize(IndexType type);

        Ref<GpuBuffer> Acquire(const BufferKey& key);
        void Release(const BufferKey& key, Ref<GpuBuffer>&& buffer);
        void MakeReady();
        void TrimLocked();

        uint32_t m_FramesInFlight = 2;
        uint64_t m_CurrentFrame = 0;
        uint64_t m_RetainedByteBudget = 0;
        mutable std::mutex m_Mutex;
        UnorderedMap<BufferKey, Vector<Ref<GpuBuffer>>, BufferKeyHash> m_Available;
        Vector<Entry> m_Retired;
        GpuBufferPoolStats m_Stats;
    };
} // namespace Crowny

#pragma once

#include "Crowny/Common/RefCounted.h"
#include "Crowny/RenderAPI/CommandBuffer.h"

namespace Crowny
{
    // A copy recorded in command order. Poll on the render thread; TryRead never
    // submits work or waits, and the source can be reused after recording it.
    class GpuBufferReadback : public RefCounted
    {
    public:
        virtual ~GpuBufferReadback() = default;
        virtual bool TryRead(void* destination, uint32_t length) = 0;
    };

    class GpuBuffer : public RefCounted
    {
    public:
        virtual ~GpuBuffer() = default;
        virtual void* Map(uint32_t offset, uint32_t length, GpuLockOptions options, uint32_t queueIdx = 0) { return nullptr; }
        virtual void Unmap() {};

        virtual void WriteData(uint32_t offset, uint32_t lenth, const void* src, BufferWriteOptions writeOptions = BWT_NORMAL) = 0;
        virtual void ReadData(uint32_t offset, uint32_t length, void* dest) = 0;
        // Unsupported backends return nullptr. The request owns its staging data
        // until released, including when released before GPU completion.
        virtual Ref<GpuBufferReadback> QueueReadback(uint32_t offset, uint32_t length) { return nullptr; }

        // TODO: Implement this properly
        virtual void CopyData(GpuBuffer& src, uint32_t srcOffset, uint32_t dstOffset, uint32_t length, bool discard = false,
                              const Ref<CommandBuffer>& commandBuffer = nullptr) {};
        void CopyData(GpuBuffer& src, const Ref<CommandBuffer>& cmdBuffer = nullptr);

        void* Lock(uint32_t offset, uint32_t length, GpuLockOptions lockOptions);
        void Unlock();

        uint32_t GetSize() const
        {
            // CW_ENGINE_ASSERT(false);
            return m_Size;
        }
        bool IsLocked() const { return m_IsLocked; }

    protected:
        GpuBuffer() = default;
        GpuBuffer(uint32_t size, BufferUsage usage);

    private:
        bool m_IsLocked = false;

    protected:
        uint32_t m_Size;
        BufferUsage m_Usage;
    };
} // namespace Crowny

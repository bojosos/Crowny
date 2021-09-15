#pragma once

#include "Platform/Vulkan/VulkanGpuBuffer.h"
#include "Platform/Vulkan/VulkanUtils.h"

#include "Crowny/RenderAPI/IndexBuffer.h"

namespace Crowny
{

    class VulkanIndexBuffer : public IndexBuffer
    {
    public:
        VulkanIndexBuffer(uint32_t count, IndexType indexType, BufferUsage usage);
        VulkanIndexBuffer(uint16_t* indices, uint32_t count, BufferUsage usage);
        VulkanIndexBuffer(uint32_t* indices, uint32_t count, BufferUsage usage);
        ~VulkanIndexBuffer();

        VkBuffer GetHandle() const { return m_Buffer->GetHandle(); }

        virtual void Bind() const override {}

        virtual void Unbind() const override {}

        virtual uint32_t GetCount() const override { return m_Count; }

        virtual void* Map(uint32_t offset, uint32_t size, GpuLockOptions options) override;
        virtual void Unmap() override;

        virtual IndexType GetIndexType() const override { return m_IndexType; }
        VulkanBuffer* GetBuffer() const { return m_Buffer->GetBuffer(); }

    private:
        VulkanGpuBuffer* m_Buffer;
        uint32_t m_Count;
        IndexType m_IndexType;
    };

} // namespace Crowny
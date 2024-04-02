#pragma once

#include "Crowny/RenderAPI/VertexBuffer.h"
#include "Platform/Vulkan/VulkanGpuBuffer.h"

namespace Crowny
{
    class VulkanVertexBuffer : public VertexBuffer
    {
    public:
        VulkanVertexBuffer(uint32_t size, BufferUsage usage);
        VulkanVertexBuffer(void* vertices, uint32_t size, BufferUsage usage);
        ~VulkanVertexBuffer();

        virtual void Bind() const override{};
        virtual void Unbind() const override{};

        virtual const BufferLayout& GetLayout() const override { return m_Layout; };
        virtual void SetLayout(const BufferLayout& layout) override { m_Layout = layout; }

        virtual void WriteData(uint32_t offset, uint32_t length, const void* src,
                               BufferWriteOptions writeOptions /* = BWT_NORMAL */) override
        {
            m_Buffer->WriteData(offset, length, src, writeOptions);
        }

        virtual void ReadData(uint32_t offset, uint32_t length, void* dest) override
        {
            m_Buffer->ReadData(offset, length, dest);
        }

        virtual void* Map(uint32_t offset, uint32_t size, GpuLockOptions options) override;
        virtual void Unmap() override;

        virtual uint32_t GetBufferSize() const override { return m_Buffer->GetSize(); }

        VkBuffer GetHandle() const { return m_Buffer->GetHandle(); }
        VulkanBuffer* GetBuffer() const { return m_Buffer->GetBuffer(); }

    private:
        VulkanGpuBuffer* m_Buffer;
        BufferUsage m_Usage;
        BufferLayout m_Layout;
    };

} // namespace Crowny
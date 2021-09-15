#pragma once

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

        virtual void* Map(uint32_t offset, uint32_t size, GpuLockOptions options) override;
        virtual void Unmap() override;

        VkBuffer GetHandle() const { return m_Buffer->GetHandle(); }
        VulkanBuffer* GetBuffer() const { return m_Buffer->GetBuffer(); }

    private:
        VulkanGpuBuffer* m_Buffer;
        BufferUsage m_Usage;
        BufferLayout m_Layout;
    };

} // namespace Crowny
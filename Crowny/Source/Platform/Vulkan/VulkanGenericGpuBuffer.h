#pragma once

#include "Crowny/RenderAPI/GenericGpuBuffer.h"
#include "Platform/Vulkan/VulkanGpuBuffer.h"

#include <vulkan/vulkan.h>

namespace Crowny
{
    class VulkanGpuBuffer;
    class VulkanBuffer;

    class VulkanGenericGpuBuffer : public GenericGpuBuffer
    {
    public:
        friend class GenericGpuBuffer;
        ~VulkanGenericGpuBuffer();

        virtual void WriteData(uint32_t offset, uint32_t length, const void* src, BufferWriteOptions writeOptions /* = BWT_NORMAL */) override;
        virtual void ReadData(uint32_t offset, uint32_t length, void* dest) override;

        virtual void* Map(uint32_t offset, uint32_t size, GpuLockOptions options) override;
        virtual void Unmap() override;

        virtual uint32_t GetBufferSize() const override { return m_Buffer->GetSize(); }
        BufferUsage GetUsage() const { return m_Usage; }

        VulkanBuffer* GetBuffer() const { return m_Buffer->GetBuffer(); }
        VkBufferView GetView() const { return m_BufferView; }
        void UpdateViews();

    protected:
        VulkanGenericGpuBuffer(uint32_t elementCount, uint32_t elementSize, GpuBufferType type, GpuBufferFormat format, BufferUsage usage);

    private:
        VulkanGpuBuffer* m_Buffer;
        BufferUsage m_Usage;

        VkBufferView m_BufferView;
        VkBuffer m_CacheBuffer;
        GpuBufferType m_BufferType;
        GpuBufferFormat m_BufferFormat;
    };
} // namespace Crowny
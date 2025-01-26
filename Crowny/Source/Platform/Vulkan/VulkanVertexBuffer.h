#pragma once

#include "Crowny/Common/Module.h"
#include "Crowny/RenderAPI/VertexBuffer.h"
#include "Platform/Vulkan/VulkanGpuBuffer.h"

namespace Crowny
{

    class VulkanBufferLayout
    {
    public:
        VulkanBufferLayout(uint32_t id, const VkPipelineVertexInputStateCreateInfo& createInfo);
        const VkPipelineVertexInputStateCreateInfo& GetVkCreateInfo() const { return m_CreateInfo; }
        uint32_t GetId() const { return m_Id; }

    private:
        uint32_t m_Id;
        VkPipelineVertexInputStateCreateInfo m_CreateInfo;
    };

    class VulkanBufferLayoutManager : public Module<VulkanBufferLayoutManager>
    {
    public:
        struct BufferLayoutKey
        {
            uint32_t BufferId;
            uint32_t ShaderId;
        };

        struct HashFunc
        {
            size_t operator()(const BufferLayoutKey& key) const;
        };

        struct EqualFunc
        {
            bool operator()(const BufferLayoutKey& a, const BufferLayoutKey& b) const;
        };

        struct BufferLayoutEntry
        {
            VkVertexInputAttributeDescription* Attributes;
            VkVertexInputBindingDescription* Bindings;
            Ref<VulkanBufferLayout> BufferLayout;
            uint32_t LastUsedIdx;
            // Allocator maybe?
        };

    public:
        VulkanBufferLayoutManager();
        ~VulkanBufferLayoutManager();

        Ref<VulkanBufferLayout> GetBufferLayout(const Ref<BufferLayout>& meshLayout, const Ref<BufferLayout>& shaderLayout);

    private:
        using BufferLayoutMap = UnorderedMap<BufferLayoutKey, BufferLayoutEntry, HashFunc, EqualFunc>;
        BufferLayoutMap::iterator AddNew(const Ref<BufferLayout>& meshLayout, const Ref<BufferLayout>& shaderLayout);

        void RemoveLeastUsed();

    private:
        static constexpr int DECL_CACHE_SIZE = 1024;
        static constexpr int NUM_MAX_PRUNE = 64;

        BufferLayoutMap m_BufferLayoutMap;

        uint32_t m_NextId;
        bool m_WarningShown;
        uint32_t m_LastUsedCounter;
        Mutex m_Mutex;
    };

    class VulkanVertexBuffer : public VertexBuffer
    {
    public:
        VulkanVertexBuffer(uint32_t size, BufferUsage usage);
        VulkanVertexBuffer(void* vertices, uint32_t size, BufferUsage usage);
        ~VulkanVertexBuffer();

        virtual void Bind() const override{};
        virtual void Unbind() const override{};

        virtual const Ref<BufferLayout>& GetLayout() const override { return m_Layout; };
        virtual void SetLayout(const Ref<BufferLayout>& layout) override { m_Layout = layout; }

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
        Ref<BufferLayout> m_Layout;
    };

} // namespace Crowny
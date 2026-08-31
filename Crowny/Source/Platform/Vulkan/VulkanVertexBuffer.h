#pragma once

#include "Crowny/Common/Module.h"
#include "Crowny/RenderAPI/VertexBuffer.h"
#include "Platform/Vulkan/VulkanGpuBuffer.h"

namespace Crowny
{
    class VulkanVertexBuffer;

    class VulkanBufferLayout : public RefCounted
    {
    public:
        VulkanBufferLayout(uint32_t id, Vector<VkVertexInputAttributeDescription> attributes,
                           Vector<VkVertexInputBindingDescription> bindings, uint32_t fallbackColorBinding);
        VulkanBufferLayout(const VulkanBufferLayout&) = delete;
        VulkanBufferLayout(VulkanBufferLayout&&) = delete;
        VulkanBufferLayout& operator=(const VulkanBufferLayout&) = delete;
        VulkanBufferLayout& operator=(VulkanBufferLayout&&) = delete;
        const VkPipelineVertexInputStateCreateInfo& GetVkCreateInfo() const { return m_CreateInfo; }
        uint32_t GetId() const { return m_Id; }
        uint32_t GetFallbackColorBinding() const { return m_FallbackColorBinding; }

    private:
        uint32_t m_Id;
        Vector<VkVertexInputAttributeDescription> m_Attributes;
        Vector<VkVertexInputBindingDescription> m_Bindings;
        VkPipelineVertexInputStateCreateInfo m_CreateInfo{};
        uint32_t m_FallbackColorBinding = UINT32_MAX;
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
            Ref<VulkanBufferLayout> BufferLayout;
            uint32_t LastUsedIdx;
        };

    public:
        VulkanBufferLayoutManager();
        explicit VulkanBufferLayoutManager(const VkPhysicalDeviceLimits& limits);
        ~VulkanBufferLayoutManager();

        Ref<VulkanBufferLayout> GetBufferLayout(const Ref<BufferLayout>& meshLayout, const Ref<BufferLayout>& shaderLayout);
        const Ref<VulkanVertexBuffer>& GetFallbackColorBuffer() const { return m_FallbackColorBuffer; }

    protected:
        void OnStartUp() override;

    private:
        using BufferLayoutMap = UnorderedMap<BufferLayoutKey, BufferLayoutEntry, HashFunc, EqualFunc>;
        BufferLayoutMap::iterator AddNew(const Ref<BufferLayout>& meshLayout, const Ref<BufferLayout>& shaderLayout);

        void ApplyLimits(const VkPhysicalDeviceLimits& limits);
        void RemoveLeastUsed();

    private:
        static constexpr int DECL_CACHE_SIZE = 1024;
        static constexpr int NUM_MAX_PRUNE = 64;

        BufferLayoutMap m_BufferLayoutMap;

        uint32_t m_NextId;
        bool m_WarningShown;
        uint32_t m_LastUsedCounter;
        uint32_t m_MaxVertexInputAttributes = 16;
        uint32_t m_MaxVertexInputBindings = 16;
        uint32_t m_MaxVertexInputAttributeOffset = 2047;
        uint32_t m_MaxVertexInputBindingStride = 2048;
        Ref<VulkanVertexBuffer> m_FallbackColorBuffer;
        Mutex m_Mutex;
    };

    class VulkanVertexBuffer : public VertexBuffer
    {
    public:
        friend class VertexBuffer;
        ~VulkanVertexBuffer();

        virtual const Ref<BufferLayout>& GetLayout() const override { return m_Layout; };
        virtual void SetLayout(const Ref<BufferLayout>& layout) override { m_Layout = layout; }

        virtual void WriteData(uint32_t offset, uint32_t length, const void* src, BufferWriteOptions writeOptions /* = BWT_NORMAL */) override
        {
            m_Buffer->WriteData(offset, length, src, writeOptions);
        }

        virtual void ReadData(uint32_t offset, uint32_t length, void* dest) override { m_Buffer->ReadData(offset, length, dest); }
        virtual void CopyData(GpuBuffer& src, uint32_t srcOffset, uint32_t dstOffset, uint32_t length, bool discard = false,
                              const Ref<CommandBuffer>& commandBuffer = nullptr) override;

        virtual void* Map(uint32_t offset, uint32_t size, GpuLockOptions options) override;
        virtual void Unmap() override;

        virtual uint32_t GetBufferSize() const override { return m_Buffer->GetSize(); }

        VkBuffer GetHandle() const { return m_Buffer->GetHandle(); }
        VulkanBuffer* GetBuffer() const { return m_Buffer->GetBuffer(); }

    protected:
        VulkanVertexBuffer(uint32_t size, BufferUsage usage);
        VulkanVertexBuffer(void* vertices, uint32_t size, BufferUsage usage);

    private:
        VulkanGpuBuffer* m_Buffer; // TODO: Move this buffer and its usage into the base gpu buffer class. It's used heavily in all ancestors.
        BufferUsage m_Usage;
        Ref<BufferLayout> m_Layout;
    };

} // namespace Crowny

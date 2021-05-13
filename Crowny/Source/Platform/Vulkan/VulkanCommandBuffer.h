#pragma once

#include "Platform/Vulkan/VulkanUtils.h"

#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanPipeline.h"
#include "Platform/Vulkan/VulkanFramebuffer.h"
#include "Platform/Vulkan/VulkanVertexBuffer.h"
#include "Platform/Vulkan/VulkanIndexBuffer.h"

#include "Crowny/Common/Module.h"
#include "Crowny/Renderer/VertexBuffer.h"
#include "Crowny/Renderer/CommandBuffer.h"

namespace Crowny
{

    class VulkanCmdBuffer;
    class VulkanCommandBufer;
    class VulkanGraphicsPipeline;
    class VulkanComputePipeline;
    
    class VulkanSemaphore
    {
    public:
        VulkanSemaphore();
        ~VulkanSemaphore();
        VkSemaphore GetHandle() const { return m_Semaphore; }
        friend class VulkanCommandBuffer;
    private:
        VkSemaphore m_Semaphore;        
    };
    
    class VulkanTransferManager;
    
    class VulkanTransferBuffer
    {
    public:
        VulkanTransferBuffer() = default;
        VulkanTransferBuffer(VulkanDevice* device, GpuQueueType type, uint32_t queueIdx);
        ~VulkanTransferBuffer();

        void AppendMask(uint32_t syncMask) { m_SyncMask |= syncMask; }
        void ClearMask() { m_SyncMask = 0; }

        void MemoryBarrier(VkBuffer buffer, VkAccessFlags srcAccessFlags, VkAccessFlags dstAccessFlags, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);

        void Flush(bool wait);
        VulkanCommandBuffer* GetCB() const { return m_CommandBuffer; }

    private:
        friend class VulkanTransferManager;

        void Allocate();

        VulkanDevice* m_Device = nullptr;
        GpuQueueType m_Type = GRAPHICS_QUEUE;
        uint32_t m_QueueIdx = 0;
        VulkanQueue* m_Queue = nullptr;
        uint32_t m_QueueMask = 0;
        VulkanCommandBuffer* m_CommandBuffer = nullptr;
        uint32_t m_SyncMask = 0;
    };
    
    class VulkanTransferManager : public Module<VulkanTransferManager>
    {
    public:
        VulkanTransferManager();
        ~VulkanTransferManager() = default;
        VulkanTransferBuffer* GetTransferBuffer(GpuQueueType type, uint32_t queueIdx);
        void FlushTransferBuffers();

    private:
        VulkanTransferBuffer m_TransferBuffers[QUEUE_COUNT][MAX_QUEUES_PER_TYPE];
    };

    class VulkanCommandBufferPool
    {
    public:
        VulkanCommandBufferPool(VulkanDevice& device);
        ~VulkanCommandBufferPool();

        VulkanCommandBuffer* GetBuffer(uint32_t queueFamily, bool secondary);        
    private:
        struct PoolInfo
        {
            VkCommandPool Pool = VK_NULL_HANDLE;
            VulkanCommandBuffer* Buffers[MAX_VULKAN_CB_PER_QUEUE_FAMILY];
            uint32_t QueueFamily = -1;
        };
        
        VulkanCommandBuffer* CreateBuffer(uint32_t queueFamily, bool secondary);
        
        VulkanDevice& m_Device;
        std::unordered_map<uint32_t, PoolInfo> m_Pools;
        uint32_t m_NextId = 1;
    };
    
    class VulkanCommandBuffer
    {
        enum class State
        {
            Ready,
            Recording,
            RecordingRenderPass,
            RecordingDone,
            Submitted
        };

    public:
        VulkanCommandBuffer(VulkanDevice& device, uint32_t id, VkCommandPool pool, uint32_t queueFamily, bool secondary);
        ~VulkanCommandBuffer();
        CommandBufferState GetState() const;
        void Reset();
        void Begin();
        void BeginRenderPass();
        void EndRenderPass();
        void End();
        uint32_t GetQueueFamily() const { return m_QueueFamily; }
        void Submit(bool transfer = false);
        VkCommandBuffer GetHandle() const { return m_CmdBuffer; }
        VkFence GetFence() const { return m_Fence; }
        void SetIsSubmitted() { m_State = State::Submitted; }

        bool CheckFenceStatus(bool block) const;
        VkSemaphore AllocateSemaphores(VkSemaphore* semaphores);
        VulkanSemaphore* GetRenderCompleteSemaphore() const { return m_Semaphore; }
        void SetRenderTarget(const Ref<Framebuffer>& target);
        void ClearRenderTarget(uint32_t buffers, const glm::vec4& color, float depth);
        void ClearViewport(uint32_t bufers, const glm::vec4& color, float depth);
        void ExecuteClearPass();
        void SetPipeline(const Ref<GraphicsPipeline>& pipeline);
        void SetPipeline(const Ref<ComputePipeline>& pipeline);
        //void SetUniforms(const Ref<UniformBuffer>& uniforms);
        bool BindGraphicsPipeline();
        void BindUniforms();
        void SetViewport(const Rect2F& area);
        void SetScrissorRect(const Rect2I& area);
        void SetDrawMode(DrawMode drawMode);
        void SetVertexBuffers(uint32_t idx, Ref<VertexBuffer>* bufffers, uint32_t numBuffers);
        void SetIndexBuffer(const Ref<IndexBuffer>& buffer);
        void SetLayout(VkImage image, VkAccessFlags srcAccessFlags, VkAccessFlags dstAccessFlags, VkImageLayout oldLayout, VkImageLayout newLayout, const VkImageSubresourceRange& range);
        void MemoryBarrier(VkBuffer buffer, VkAccessFlags srcAccessFlags, VkAccessFlags dstAccessFlags, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);
        void Draw(uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount);
        void DrawIndexed(uint32_t startIdx, uint32_t idxCount, uint32_t vertexOffset, uint32_t instanceCount);
        bool IsReadyForRender() const;
        bool IsSubmitted() const { return m_State == State::Submitted; } 
        bool IsReadyForSubmit() const { return m_State == State::RecordingDone; }
        bool IsRecording() const { return m_State == State::Recording; }
        bool IsInRenderPass() const { return m_State == State::RecordingRenderPass; }
        
    private:
        friend class VulkanCommandBufferPool;
        void BindDynamicStates(bool force);
        void BindVertexInputs();

        bool m_ViewportRequiresBind : 1;
        bool m_ScissorRequiresBind : 1;
        bool m_GraphicsPipelineRequiresBind : 1;
        bool m_ComputePipelineRequiresBind : 1;
        bool m_VertexInputsRequriesBind : 1;
        
        std::array<VkClearValue, 8> m_ClearValues{};
        Rect2I m_ClearArea;
        uint32_t m_QueueFamily;
        uint32_t m_Id;

        bool m_RenderTargetModified = false;
        VkFence m_Fence;
        Rect2F m_Viewport;
        Rect2I m_Scissor;
        VkDeviceSize m_VertexBufferOffsets[16] {}; //TODO: do not hardcode 16 here
        DrawMode m_DrawMode = DrawMode::TRIANGLE_LIST;
        VkDescriptorSet* m_DescriptorSetsTemp;
        std::unordered_map<uint32_t, TransitionInfo> m_TransitionInfoTemp;
        //VulkanSemaphore* m_Semaphore;
        std::vector<VulkanSemaphore*> m_SemaphoresTemp { }; // TODO: max queues
        VkBuffer m_VertexBuffersTemp[16] = {}; // TODO: max vertex buffers
        Ref<VulkanIndexBuffer> m_IndexBuffer;
        std::vector<Ref<VulkanVertexBuffer>> m_VertexBuffers;
        VulkanFramebuffer* m_RenderTarget = nullptr;
        Ref<VulkanGraphicsPipeline> m_GraphicsPipeline;
        Ref<VulkanComputePipeline> m_ComputePipeline;
        VulkanSwapChain* m_SwapChain;
        
        VulkanSemaphore* m_Semaphore; // render complete semaphore
        VkCommandPool m_Pool;
        VkCommandBuffer m_CmdBuffer;
        VulkanQueue* m_Queue;
        VulkanDevice& m_Device;
        State m_State = State::Ready;
        friend class VulkanCmdBuffer;
        
    };

    class VulkanCmdBuffer : public CommandBuffer
    {
    public:
        VulkanCmdBuffer(GpuQueueType queueType);
        ~VulkanCmdBuffer();
        VulkanCommandBuffer* GetBuffer() const { return m_Buffer; }
        virtual CommandBufferState GetState() const override {};
        virtual void Reset() override { m_Buffer->Reset(); };
    private:
        VulkanCommandBuffer* m_Buffer;
    };
    
    enum class DescriptorSetBindFlag
    {
        None = 0,
        Graphics = 1 << 0,
        Compute = 1 << 1
    };
    
    enum class ImageUsageFlagBits
    {
        Shader = 1 << 0,
        Framebuffer = 1 << 1,
        Transfer = 1 << 2
    };

}
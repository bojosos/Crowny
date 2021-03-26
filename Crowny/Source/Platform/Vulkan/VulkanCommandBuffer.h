#pragma once

#include "Platform/Vulkan/VulkanUtils.h"

#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanPipeline.h"
#include "Platform/Vulkan/VulkanFramebuffer.h"
#include "Platform/Vulkan/VulkanVertexBuffer.h"
#include "Crowny/Renderer/IndexBuffer.h"
#include "Crowny/Renderer/VertexBuffer.h"

namespace Crowny
{

    class VulkanCommandBuffer;
    
    class VulkanSemaphore
    {
    public:
        VulkanSemaphore();
        ~VulkanSemaphore();
        VkSemaphore GetHandle() const { return m_Semaphore; }
        
    private:
        VkSemaphore m_Semaphore;        
    };

    class VulkanCommandBuffer;

    class VulkanCommandBufferPool
    {
    public:
        VulkanCommandBufferPool(VulkanDevice& device);
        ~VulkanCommandBufferPool();

        VulkanCommandBuffer GetBuffer(uint32_t queueFamily, bool secondary);        
    private:
        struct PoolInfo
        {
            VkCommandPool Pool = VK_NULL_HANDLE;
            VulkanCommandBuffer* Buffers[64];
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
        void Begin();
        void BeginRenderPass();
        void EndRenderPass();
        void End();
        uint32_t GetQueueFamily() const { return m_QueueFamily; }
        void Submit(VkQueue* queue, uint32_t queueIdx);
        VkCommandBuffer GetHandle() const { return m_CmdBuffer; }
        VkFence GetFence() const { return m_Fence; }
        void SetIsSubmitted() { m_State = State::Submitted; }

        void AllocateSemaphores(VkSemaphore* semaphores);
        void SetRenderTarget(const Ref<Framebuffer>& target);
        void ClearRenderTarget(uint32_t buffers, const glm::vec4& color, float depth);
        void ClearViewport(uint32_t bufers, const glm::vec4& color, float depth);
        void ExecuteClearPass();
        void SetPipeline(const Ref<GraphicsPipeline>& pipeline);
        void SetPipeline(const Ref<ComputePipeline>& pipeline);
        //void SetUniforms(const Ref<UniformBuffer>& uniforms);
        bool BindGraphicsPipeline();
        bool IsReadyForRender() const;
        void BindUniforms();
        void SetViewport(const Rect2F& area);
        void SetScrissorRect(const Rect2I& area);
        void SetDrawMode(DrawMode drawMode);
        void SetVertexBuffers(uint32_t idx, Ref<VertexBuffer>* bufffers, uint32_t numBuffers);
        void SetIndexBuffer(const Ref<IndexBuffer>& buffer);
        void SetLayout(VkImage image, VkAccessFlags srcAccessFlags, VkAccessFlags dstAccessFlags, VkImageLayout oldLayout, VkImageLayout newLayout, const VkImageSubresourceRange& range);
        void MemoryBarrier(VkBuffer buffer, VkAccessFlags srcAccessFlags, VkAccessFlags dstAccessFlags, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);
        
    private:
        friend class VulkanCommandBufferPool;
        void Draw(uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount);
        void DrawIndexed(uint32_t startIdx, uint32_t idxCount, uint32_t vertexOffset, uint32_t instanceCount);
        void BindDynamicStates(bool force);
        void BindVertexInputs();
        
        bool IsInRenderPass() const { return m_State == State::RecordingRenderPass; }
        bool IsReadyForSubmit() const { return m_State == State::RecordingDone; }
        bool IsRecording() const { return m_State == State::Recording; }
        bool IsSubmitted() const { return m_State == State::Submitted; }

        void Reset();
        
        bool m_ViewportRequiresBind = true, m_ScissorRequiresBind = true;
        bool m_GraphicsPipelineRequiresBind = true;
        bool m_ComputePipelineRequiresBind = true;
        bool m_VertexInputsRequriesBind = true;
        std::array<VkClearValue, 8> m_ClearValues{};
        Rect2I m_ClearArea;
        uint32_t m_QueueFamily;

        bool m_RenderTargetModified = true;
        VkFence m_Fence;
        Rect2F m_Viewport;
        Rect2I m_Scissor;
        DrawMode m_DrawMode = DrawMode::TRIANGLE_LIST;
        VkDescriptorSet* m_DescriptorSetsTemp;
        std::unordered_map<uint32_t, TransitionInfo> m_TransitionInfoTemp;
        std::vector<VulkanSemaphore*> m_SemaphoresTemp { }; // TODO: max queues
        VkBuffer m_VertexBuffersTemp[] {}; // TODO: max vertex buffers
        VulkanIndexBuffer m_IndexBuffer;
        std::vector<VulkanVertexBuffer> m_VertexBuffers;
        Ref<VulkanFramebuffer> m_RenderTarget;
        Ref<VulkanGraphicsPipeline> m_GraphicsPipeline;
        Ref<VulkanComputePipeline> m_ComputePipeline;
        
        VkCommandBuffer m_CmdBuffer;
        VulkanQueue* m_Queue;
        VulkanDevice& m_Device;
        State m_State;
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
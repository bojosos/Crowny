#pragma once

#include "Crowny/Common/Module.h"
#include "Crowny/RenderAPI/CommandBuffer.h"
#include "Platform/Vulkan/VulkanResource.h"
#include "Platform/Vulkan/VulkanUtils.h"

namespace Crowny
{

#define MAX_VULKAN_CB_DEPENDENCIES 2
#define MAX_VULKAN_CB_PER_QUEUE_FAMILY MAX_QUEUES_PER_TYPE * 32

    class VulkanBuffer;
    class VulkanCmdBuffer;
    class VulkanCommandBufer;
    class VulkanGraphicsPipeline;
    class VulkanComputePipeline;
    class VulkanVertexBuffer;
    class VulkanIndexBuffer;
    class VulkanUniformParams;
    class VulkanImage;
    class VulkanTimerQuery;
    class VulkanPipelineQuery;
    class VulkanOcclusionQuery;
    class VulkanQuery;

    enum class BufferUseFlagBits
    {
        Generic = 1 << 0,
        Index = 1 << 1,
        Vertex = 1 << 2,
        Uniform = 1 << 3,
        Transfer = 1 << 4
    };
    typedef Flags<BufferUseFlagBits> BufferUseFlags;
    CW_FLAGS_OPERATORS(BufferUseFlagBits);

    enum class DescriptorSetBindFlagBits
    {
        None = 0 << 0,
        Graphics = 1 << 0,
        Compute = 1 << 1
    };
    typedef Flags<DescriptorSetBindFlagBits> DescriptorSetBindFlags;
    CW_FLAGS_OPERATORS(DescriptorSetBindFlagBits);

    enum class ImageUseFlagBits
    {
        Shader = 1 << 0,
        Framebuffer = 1 << 1,
        Transfer = 1 << 2
    };
    typedef Flags<ImageUseFlagBits> ImageUseFlags;
    CW_FLAGS_OPERATORS(ImageUseFlagBits);

    class VulkanSemaphore : public VulkanResource
    {
    public:
        VulkanSemaphore(VulkanResourceManager* owner);
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

        void MemoryBarrier(VkBuffer buffer, VkAccessFlags srcAccessFlags, VkAccessFlags dstAccessFlags,
                           VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);
        void SetLayout(VkImage image, VkAccessFlags srcAccessFlags, VkAccessFlags dstAccessFlags,
                       VkImageLayout oldLayout, VkImageLayout newLayout, const VkImageSubresourceRange& range);
        void SetLayout(VulkanImage* image, const VkImageSubresourceRange& range, VkAccessFlags newAccessMask,
                       VkImageLayout newLayout);

        void Flush(bool wait);
        VulkanCmdBuffer* GetCB() const { return m_CommandBuffer; }

    private:
        friend class VulkanTransferManager;

        void Allocate();

        VulkanDevice* m_Device = nullptr;
        GpuQueueType m_Type = GRAPHICS_QUEUE;
        uint32_t m_QueueIdx = 0;
        VulkanQueue* m_Queue = nullptr;
        uint32_t m_QueueMask = 0;
        VulkanCmdBuffer* m_CommandBuffer = nullptr;
        uint32_t m_SyncMask = 0;
        Vector<VkImageMemoryBarrier> m_BarriersTemp;
    };

    class VulkanTransferManager : public Module<VulkanTransferManager>
    {
    public:
        VulkanTransferManager();
        ~VulkanTransferManager() = default;
        VulkanTransferBuffer* GetTransferBuffer(GpuQueueType type, uint32_t queueIdx);
        void FlushTransferBuffers();

        void GetSyncSemaphores(uint32_t syncMask, VulkanSemaphore** semaphores, uint32_t& count);

    private:
        VulkanTransferBuffer m_TransferBuffers[QUEUE_COUNT][MAX_QUEUES_PER_TYPE];
    };

    class VulkanCommandBufferPool
    {
    public:
        VulkanCommandBufferPool(VulkanDevice& device);
        ~VulkanCommandBufferPool();

        VulkanCmdBuffer* GetBuffer(uint32_t queueFamily, bool secondary);

    private:
        struct PoolInfo
        {
            VkCommandPool Pool = VK_NULL_HANDLE;
            VulkanCmdBuffer* Buffers[MAX_VULKAN_CB_PER_QUEUE_FAMILY];
            uint32_t QueueFamily = -1;
        };

        VulkanCmdBuffer* CreateBuffer(uint32_t queueFamily, bool secondary);

        VulkanDevice& m_Device;
        UnorderedMap<uint32_t, PoolInfo> m_Pools;
        uint32_t m_NextId = 1;
    };

    class VulkanCmdBuffer
    {
        enum class State
        {
            Ready,
            Recording,
            RecordingRenderPass,
            RecordingDone,
            Submitted
        };

    private:
        struct ResourceUseHandle
        {
            bool Used;
            VulkanAccessFlags Flags;
        };

        struct ResourcePipelineUse
        {
            VulkanAccessFlags AccessFlags;
            VkPipelineStageFlags Stages = 0;
        };

        struct BufferInfo
        {
            ResourceUseHandle UseHandle;
            BufferUseFlags UseFlags;
            ResourcePipelineUse WriteHazardUse;
        };

        struct ImageInfo
        {
            ResourceUseHandle UseHandle;
            uint32_t SubresourceInfoIdx;
            uint32_t NumSubresourceInfos;
        };

        struct ImageSubresourceInfo
        {
            VkImageSubresourceRange Range;
            ResourcePipelineUse ShaderUse;
            ResourcePipelineUse FbUse;
            ResourcePipelineUse TransferUse;
            ResourcePipelineUse WriteHazardUse;
            ImageUseFlags UseFlags;
            bool InitialReadOnly = false;
            VkImageLayout InitialLayout;
            VkImageLayout CurrentLayout;
            VkImageLayout RequiredLayout;
            VkImageLayout RenderPassLayout;
        };

    public:
        VulkanCmdBuffer(VulkanDevice& device, uint32_t id, VkCommandPool pool, uint32_t queueFamily, bool secondary);
        ~VulkanCmdBuffer();
        CommandBufferState GetState() const;
        void Reset();
        void Begin();
        void BeginRenderPass();
        void EndRenderPass();
        void End();
        uint32_t GetQueueFamily() const { return m_QueueFamily; }
        void Submit(VulkanQueue* queue, uint32_t queueIdx, uint32_t syncMask);
        VkCommandBuffer GetHandle() const { return m_CmdBuffer; }
        VkFence GetFence() const { return m_Fence; }
        VulkanSemaphore* GetIntraQueueSemaphore() const { return m_IntraQueueSemaphore; }
        void AllocateSemaphores(VkSemaphore* semaphores);
        VulkanSemaphore* RequestInterQueueSemaphore() const;

        void SetIsSubmitted() { m_State = State::Submitted; }

        void RegisterResource(VulkanResource* resource, VulkanAccessFlags flags);
        void RegisterResource(VulkanFramebuffer* framebuffer, RenderSurfaceMask loadMask, uint32_t readMask);
        void RegisterResource(VulkanSwapChain* swapChain);
        void RegisterResource(VulkanImage* image, const VkImageSubresourceRange& range, ImageUseFlagBits use,
                              VkImageLayout layout, VkImageLayout finalLayout, VulkanAccessFlags access,
                              VkPipelineStageFlags stages);
        void RegisterBuffer(VulkanBuffer* buffer, BufferUseFlagBits useFlags, VulkanAccessFlags accessFlags,
                            VkPipelineStageFlags stages = 0);
        void RegisterImageShader(VulkanImage* image, const VkImageSubresourceRange& range, VkImageLayout layout,
                                 VulkanAccessFlags access, VkPipelineStageFlags stages);
        void RegisterImageFramebuffer(VulkanImage* image, const VkImageSubresourceRange& range, VkImageLayout layout,
                                      VkImageLayout finalLayout, VulkanAccessFlags access, VkPipelineStageFlags stages);
        void RegisterImageTransfer(VulkanImage* image, const VkImageSubresourceRange& range, VkImageLayout layout,
                                   VulkanAccessFlags access);

        void RegisterQuery(VulkanTimerQuery* query) { m_TimerQueries.insert(query); }
        void RegisterQuery(VulkanPipelineQuery* query) { m_PipelineQueries.insert(query); }
        void RegisterQuery(VulkanOcclusionQuery* query) { m_OcclusionQueries.insert(query); }

        void UpdateFinalLayouts();
        void UpdateShaderSubresource(VulkanImage* image, uint32_t imageInfoIdx, ImageSubresourceInfo& subresourceInfo,
                                     VkImageLayout layout, VulkanAccessFlags access, VkPipelineStageFlags stages);
        void UpdateFramebufferSubresource(VulkanImage* image, uint32_t imageInfoIdx,
                                          ImageSubresourceInfo& subresourceInfo, VkImageLayout layout,
                                          VkImageLayout finalLayout, VulkanAccessFlags access,
                                          VkPipelineStageFlags stages);
        void UpdateTransferSubresource(VulkanImage* image, uint32_t imageInfoIdx, ImageSubresourceInfo& subresourceInfo,
                                       VkImageLayout layout, VulkanAccessFlags access, VkPipelineStageFlags stages);
        ImageSubresourceInfo& FindSubresourceInfo(VulkanImage* image, uint32_t face, uint32_t mip);

        void ResetQuery(VulkanQuery* query);

        bool CheckFenceStatus(bool block) const;

        bool IsSubmitted() const { return m_State == State::Submitted; }
        bool IsReadyForSubmit() const { return m_State == State::RecordingDone; }
        bool IsRecording() const { return m_State == State::Recording; }
        bool IsInRenderPass() const { return m_State == State::RecordingRenderPass; }

        void SetRenderTarget(const Ref<RenderTarget>& target, uint32_t readOnlyFlags, RenderSurfaceMask loadMask);
        void ClearRenderTarget(uint32_t buffers, const glm::vec4& color, float depth);
        void ClearViewport(uint32_t buffers, const glm::vec4& color, float depth);
        void SetPipeline(const Ref<GraphicsPipeline>& pipeline);
        void SetPipeline(const Ref<ComputePipeline>& pipeline);
        void SetUniforms(const Ref<UniformParams>& uniforms);
        void SetViewport(const Rect2F& area);
        void SetScrissorRect(const Rect2I& area);
        void SetStencilRef(uint32_t value);
        void SetDrawMode(DrawMode drawMode);
        void SetVertexBuffers(uint32_t idx, Ref<VertexBuffer>* bufffers, uint32_t numBuffers);
        void SetVertexLayout(const Ref<BufferLayout>& bufferLayout);
        void SetIndexBuffer(const Ref<IndexBuffer>& buffer);
        void Draw(uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount);
        void DrawIndexed(uint32_t startIdx, uint32_t idxCount, uint32_t vertexOffset, uint32_t instanceCount);
        void Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ);

        void MemoryBarrier(VkBuffer buffer, VkAccessFlags srcAccessFlags, VkAccessFlags dstAccessFlags,
                           VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);
        void SetLayout(VkImage image, VkAccessFlags srcAccessFlags, VkAccessFlags dstAccessFlags,
                       VkImageLayout oldLayout, VkImageLayout newLayout, const VkImageSubresourceRange& range);
        VkImageLayout GetCurrentLayout(VulkanImage* image, const VkImageSubresourceRange& range, bool isInRenderPass);
        void ClearRenderTarget(uint32_t buffers, const glm::vec4& color, float depth, uint16_t stencil,
                               uint8_t targetMask);
        void ClearViewport(uint32_t buffers, const glm::vec4& color, float depth, uint16_t stencil, uint8_t targetMask);

        void BindVertexInputs();

    private:
        friend class VulkanCommandBuffer;

        void BindUniforms();
        bool BindGraphicsPipeline();
        bool IsReadyForRender() const;
        void BindDynamicStates(bool force);
        void ClearViewport(const Rect2I& area, uint32_t buffers, const glm::vec4& color, float depth, uint16_t stencil,
                           uint8_t targetMask);
        void ExecuteClearPass();
        void ExecuteWriteHazardBarrier();
        void ExecuteLayoutTransitions();
        RenderSurfaceMask GetFBReadMask();
        void GetInProgressQueries(Vector<VulkanTimerQuery*>& timers, Vector<VulkanPipelineQuery*>& pipelines,
                                  Vector<VulkanOcclusionQuery*>& occlusions) const;

    private:
        friend class VulkanCommandBufferPool;

        bool m_NeedsWarMemoryBarrier : 1;
        bool m_NeedsRawMemoryBarrier : 1;
        VkPipelineStageFlags m_MemoryBarrierSrcStages = 0;
        VkPipelineStageFlags m_MemoryBarrierDstStages = 0;
        VkAccessFlags m_MemoryBarrierSrcAccess = 0;
        VkAccessFlags m_MemoryBarrierDstAccess = 0;
        ClearMask m_ClearMask;

        bool m_ViewportRequiresBind : 1;
        bool m_ScissorRequiresBind : 1;
        bool m_GraphicsPipelineRequiresBind : 1;
        bool m_ComputePipelineRequiresBind : 1;
        bool m_VertexInputsRequriesBind : 1;
        bool m_BoundUniformsDirty : 1;
        bool m_StencilRequriesBind : 1;
        bool m_BufferLayoutDirty : 1;

        mutable uint32_t m_NumUsedInterQueueSemaphores = 0;
        std::array<VkClearValue, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS + 1> m_ClearValues{};
        UnorderedMap<VulkanResource*, BufferInfo> m_Buffers;
        UnorderedMap<VulkanResource*, ResourceUseHandle> m_Resources;
        UnorderedMap<VulkanResource*, uint32_t> m_Images;
        UnorderedMap<VulkanSwapChain*, ResourceUseHandle> m_SwapChains;

        UnorderedSet<VulkanTimerQuery*> m_TimerQueries;
        UnorderedSet<VulkanPipelineQuery*> m_PipelineQueries;
        UnorderedSet<VulkanOcclusionQuery*> m_OcclusionQueries;

        Set<uint32_t> m_ShaderBoundSubresourceInfos;
        uint32_t m_GlobalQueueIdx = -1;

        uint32_t m_RenderTargetReadOnlyFlags = 0; // TODO Use the flags class
        RenderSurfaceMask m_RenderTargetLoadMask = RT_NONE;

        Rect2I m_ClearArea;
        uint32_t m_QueueFamily;
        uint32_t m_Id;
        uint32_t m_NumBoundDescriptorSets;

        Ref<VulkanUniformParams> m_BoundUniforms;
        bool m_RenderTargetModified = false;
        VkFence m_Fence;

        Rect2F m_Viewport = { 0.0f, 0.0f, 1.0f, 1.0f };
        Rect2I m_Scissor = { 0, 0, 0, 0 };
        uint32_t m_StencilRef = 0;

        DrawMode m_DrawMode = DrawMode::TRIANGLE_LIST;
        VkDescriptorSet* m_DescriptorSetsTemp;
        DescriptorSetBindFlags m_DescriptorSetsBindState;
        Vector<ImageSubresourceInfo> m_SubresourceInfoStorage;
        Vector<ImageInfo> m_ImageInfos;
        Vector<VkImageMemoryBarrier> m_LayoutTransitionBarriersTemp;
        Vector<Ref<VulkanVertexBuffer>> m_VertexBuffers;
        Vector<VulkanQuery*> m_QueuedQueryResets;
        UnorderedMap<uint32_t, TransitionInfo> m_TransitionInfoTemp;
        UnorderedMap<VulkanImage*, uint32_t> m_QueuedLayoutTransitions;
        Vector<VulkanSemaphore*> m_SemaphoresTemp{ MAX_UNIQUE_QUEUES };
        VkBuffer m_VertexBuffersTemp[MAX_BOUND_VERTEX_BUFFERS] = {};
        VkDeviceSize m_VertexBufferOffsets[MAX_BOUND_VERTEX_BUFFERS]{};
        Ref<VulkanIndexBuffer> m_IndexBuffer;
        VulkanFramebuffer* m_Framebuffer = nullptr;
        Ref<RenderTarget> m_RenderTarget;
        Ref<VulkanGraphicsPipeline> m_GraphicsPipeline;
        Ref<VulkanComputePipeline> m_ComputePipeline;
        VulkanDevice& m_Device;
        Set<VulkanSwapChain*> m_ActiveSwapChains;
        Ref<BufferLayout> m_VertexLayout;

        VulkanSemaphore* m_IntraQueueSemaphore = nullptr;
        VulkanSemaphore* m_InterQueueSemaphores[MAX_VULKAN_CB_DEPENDENCIES]{};
        VkCommandPool m_Pool;
        VkCommandBuffer m_CmdBuffer;
        State m_State = State::Ready;
    };

    class VulkanCommandBuffer : public CommandBuffer
    {
    public:
        VulkanCommandBuffer(VulkanDevice& device, GpuQueueType queueType, uint32_t queueIdx, bool secondary);
        VulkanCmdBuffer* GetInternal() const { return m_Buffer; }
        void Submit(uint32_t snycMask);
        virtual CommandBufferState GetState() const override;
        virtual void Reset() override;
        void AcquireNewBuffer();

    private:
        VulkanCmdBuffer* m_Buffer = nullptr;
        VulkanDevice& m_Device;
        VulkanQueue* m_Queue;
        uint32_t m_IdMask;
    };

} // namespace Crowny

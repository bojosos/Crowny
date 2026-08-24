#pragma once

#include "Crowny/RenderAPI/Query.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanResource.h"

namespace Crowny
{

    class VulkanQuery;

    class VulkanQueryPool
    {
    public:
        VulkanQueryPool(VulkanDevice& device);
        ~VulkanQueryPool();
        VulkanQuery* BeginTimerQuery(VulkanCmdBuffer* cb);
        VulkanQuery* BeginPipelineQuery(VulkanCmdBuffer* cb);
        VulkanQuery* BeginOcclusionQuery(VulkanCmdBuffer* cb);

        void EndOcclusionQuery(VulkanQuery* query, VulkanCmdBuffer* cb);
        void Release(VulkanQuery* query);

    private:
        struct PoolInfo
        {
            VkQueryPool Pool = VK_NULL_HANDLE;
            uint32_t StartIdx = 0;
        };

        VulkanQuery* GetQuery(VkQueryType type);
        PoolInfo& AllocatePool(VkQueryType type);

        static const uint32_t NUM_QUERIES_PER_POOL = 32;
        VulkanDevice& m_Device;

        Vector<VulkanQuery*> m_TimerQueries;
        Vector<VulkanQuery*> m_PipelineQueries;
        Vector<VulkanQuery*> m_OcclusionQueries;

        Vector<PoolInfo> m_TimerPools;
        Vector<PoolInfo> m_OcclusionPools;
        Vector<PoolInfo> m_PipelinePools;
    };

    class VulkanQuery : public VulkanResource
    {
    public:
        VulkanQuery(VulkanResourceManager* owner, VkQueryPool pool, uint32_t queryIdx);
        ~VulkanQuery() = default;

        bool GetResult(uint64_t& result);
        bool GetResultArray(Vector<uint64_t>& result);
        void Reset(VkCommandBuffer cmdBuffer);

    private:
        friend class VulkanQueryPool;

        VkQueryPool m_Pool;
        uint32_t m_QueryIdx;
        bool m_Free = true;
    };

    class VulkanTimerQuery : public TimerQuery
    {
    public:
        friend class TimerQuery;
        ~VulkanTimerQuery();
        virtual void Begin(const Ref<CommandBuffer>& cb) override;
        virtual void End(const Ref<CommandBuffer>& cb) override;

        virtual bool IsReady() const override;
        virtual float GetTimeMs() override;
        bool IsInProgress() const;
        void Interrupt(VulkanCmdBuffer* cb);

    protected:
        VulkanTimerQuery();

    private:
        Vector<Pair<VulkanQuery*, VulkanQuery*>> m_Queries;

        float m_TimeDelta = 0.0f;
        bool m_QueryEndCalled : 1;
        bool m_QueryFinalized : 1;
    };

    class VulkanPipelineQuery : public PipelineQuery
    {
    public:
        friend class PipelineQuery;
        // virtual void Begin(const Ref<CommandBuffer>& cb) override;
        bool IsInProgress() { return false; }
        void Interrupt(VulkanCmdBuffer* cb) {}

        virtual bool IsReady() const override { return false; }
        virtual void Begin(const Ref<CommandBuffer>& cb = nullptr) override {}
        virtual void End(const Ref<CommandBuffer>& cb = nullptr) override {};

    protected:
        VulkanPipelineQuery() = default;
    };

    class VulkanOcclusionQuery : public OcclusionQuery
    {
    public:
        friend class OcclusionQuery;
        bool IsInProgress() { return false; }
        void Interrupt(VulkanCmdBuffer* cb) {}

    protected:
        VulkanOcclusionQuery(bool binary) {}
    };
} // namespace Crowny

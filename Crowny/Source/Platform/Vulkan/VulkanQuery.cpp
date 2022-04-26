#include "cwpch.h"

#include "Platform/Vulkan/VulkanQuery.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"

namespace Crowny
{
    VulkanQuery::VulkanQuery(VulkanResourceManager* owner, VkQueryPool pool, uint32_t queryIdx)
      : VulkanResource(owner, false), m_Pool(pool), m_QueryIdx(queryIdx)
    {
    }

    bool VulkanQuery::GetResultArray(Vector<uint64_t>& result)
    {
        VkDevice vkDevice = m_Owner->GetDevice().GetLogicalDevice();
        uint32_t count = 7; // compute, vertex invokes, input assembly prims/verts, clip planes and frag
        if (gVulkanRenderAPI().GetPresentDevice()->GetDeviceFeatures().geometryShader)
            count += 2; // geometry invokes and primitives generated
        if (gVulkanRenderAPI().GetPresentDevice()->GetDeviceFeatures().tessellationShader)
            count += 2; // tessellation invokes and primitives generated

        result.resize(count);
        VkResult vkResult = vkGetQueryPoolResults(vkDevice, m_Pool, m_QueryIdx, 1, result.size() * sizeof(uint64_t),
                                                  result.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
        CW_ENGINE_ASSERT(vkResult == VK_SUCCESS || vkResult == VK_NOT_READY);
        return vkResult == VK_SUCCESS;
    }

    bool VulkanQuery::GetResult(uint64_t& result)
    {
        VkDevice vkDevice = m_Owner->GetDevice().GetLogicalDevice();
        VkResult vkResult = vkGetQueryPoolResults(vkDevice, m_Pool, m_QueryIdx, 1, sizeof(result), &result,
                                                  sizeof(result), VK_QUERY_RESULT_64_BIT);
        CW_ENGINE_ASSERT(vkResult == VK_SUCCESS || vkResult == VK_NOT_READY);
        return vkResult == VK_SUCCESS;
    }

    VulkanQueryPool::VulkanQueryPool(VulkanDevice& device) : m_Device(device)
    {
        AllocatePool(VK_QUERY_TYPE_TIMESTAMP);
        AllocatePool(VK_QUERY_TYPE_OCCLUSION);
    }

    VulkanQueryPool::~VulkanQueryPool()
    {
        for (auto entry : m_TimerQueries)
            if (entry != nullptr)
                entry->Destroy();
        for (auto entry : m_OcclusionQueries)
            if (entry != nullptr)
                entry->Destroy();
        for (auto entry : m_PipelineQueries)
            if (entry != nullptr)
                entry->Destroy();

        for (auto& entry : m_TimerPools)
            vkDestroyQueryPool(m_Device.GetLogicalDevice(), entry.Pool, gVulkanAllocator);
        for (auto& entry : m_OcclusionPools)
            vkDestroyQueryPool(m_Device.GetLogicalDevice(), entry.Pool, gVulkanAllocator);
        for (auto& entry : m_PipelinePools)
            vkDestroyQueryPool(m_Device.GetLogicalDevice(), entry.Pool, gVulkanAllocator);
    }

    VulkanQueryPool::PoolInfo& VulkanQueryPool::AllocatePool(VkQueryType type)
    {
        VkQueryPoolCreateInfo queryPoolCreateInfo;
        queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolCreateInfo.pNext = nullptr;
        queryPoolCreateInfo.flags = 0;

        VkQueryPipelineStatisticFlags flags = 0;
        uint32_t queryCount = 1;
        if (type == VK_QUERY_TYPE_PIPELINE_STATISTICS)
        {
            flags = VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;
            queryCount = 7;
            if (gVulkanRenderAPI().GetPresentDevice()->GetDeviceFeatures().geometryShader)
            {
                flags |= VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT |
                         VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT;
                queryCount += 2;
            }
            if (gVulkanRenderAPI().GetPresentDevice()->GetDeviceFeatures().tessellationShader)
            {
                queryCount += 2;
                flags |= VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT |
                         VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT;
            }
        }

        queryPoolCreateInfo.pipelineStatistics = flags;
        queryPoolCreateInfo.queryCount = queryCount;
        queryPoolCreateInfo.queryType = type;

        PoolInfo poolInfo;
        VkResult result = vkCreateQueryPool(m_Device.GetLogicalDevice(), &queryPoolCreateInfo, nullptr, &poolInfo.Pool);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        Vector<PoolInfo>& poolInfos =
          type == VK_QUERY_TYPE_TIMESTAMP
            ? m_TimerPools
            : (type == VK_QUERY_TYPE_PIPELINE_STATISTICS ? m_PipelinePools : m_OcclusionPools);
        poolInfo.StartIdx = (uint32_t)poolInfos.size() * queryCount;

        poolInfos.push_back(poolInfo);
        Vector<VulkanQuery*>& queries =
          type == VK_QUERY_TYPE_TIMESTAMP
            ? m_TimerQueries
            : (type == VK_QUERY_TYPE_PIPELINE_STATISTICS ? m_PipelineQueries : m_OcclusionQueries);
        for (uint32_t i = 0; i < queryCount; i++)
            queries.push_back(nullptr);
        return poolInfos.back();
    }

    VulkanQuery* VulkanQueryPool::GetQuery(VkQueryType type)
    {
        Vector<VulkanQuery*>& queries =
          type == VK_QUERY_TYPE_TIMESTAMP
            ? m_TimerQueries
            : (type == VK_QUERY_TYPE_PIPELINE_STATISTICS ? m_PipelineQueries : m_OcclusionQueries);
        Vector<PoolInfo>& poolInfos =
          type == VK_QUERY_TYPE_TIMESTAMP
            ? m_TimerPools
            : (type == VK_QUERY_TYPE_PIPELINE_STATISTICS ? m_PipelinePools : m_OcclusionPools);

        for (uint32_t i = 0; i < (uint32_t)queries.size(); i++)
        {
            VulkanQuery* cur = queries[i];
            if (cur == nullptr)
            {
                div_t divResult = std::div(i, (int32_t)NUM_QUERIES_PER_POOL);
                uint32_t poolIdx = (uint32_t)divResult.quot;
                uint32_t queryIdx = (uint32_t)divResult.rem;
                cur = m_Device.GetResourceManager().Create<VulkanQuery>(poolInfos[poolIdx].Pool, queryIdx);
                queries[i] = cur;
                return cur;
            }
            else if (!cur->IsBound() && cur->m_Free)
                return cur;
        }

        PoolInfo& poolInfo = AllocatePool(type);
        uint32_t queryidx = poolInfo.StartIdx % NUM_QUERIES_PER_POOL;
        VulkanQuery* query = m_Device.GetResourceManager().Create<VulkanQuery>(poolInfo.Pool, queryidx);
        queries[poolInfo.StartIdx] = query;
        return query;
    }

    VulkanQuery* VulkanQueryPool::BeginTimerQuery(VulkanCmdBuffer* cmdBuffer)
    {
        VulkanQuery* query = GetQuery(VK_QUERY_TYPE_TIMESTAMP);
        query->m_Free = false;
        VkCommandBuffer vkCmdBuffer = cmdBuffer->GetHandle();
        cmdBuffer->ResetQuery(query);
        vkCmdWriteTimestamp(vkCmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, query->m_Pool, query->m_QueryIdx);

        cmdBuffer->RegisterResource(query, VulkanAccessFlagBits::Write);
        return query;
    }

    void VulkanQueryPool::EndOcclusionQuery(VulkanQuery* query, VulkanCmdBuffer* cb)
    {
        VkCommandBuffer vkCmdBuffer = cb->GetHandle();
        vkCmdEndQuery(vkCmdBuffer, query->m_Pool, query->m_QueryIdx);
    }

    void VulkanQueryPool::Release(VulkanQuery* query) { query->m_Free = true; }

    void VulkanQuery::Reset(VkCommandBuffer cmdBuffer) { vkCmdResetQueryPool(cmdBuffer, m_Pool, m_QueryIdx, 1); }

    VulkanTimerQuery::VulkanTimerQuery() : m_QueryEndCalled(false), m_QueryFinalized(false) {}

    VulkanTimerQuery::~VulkanTimerQuery()
    {
        VulkanQueryPool& queryPool = gVulkanRenderAPI().GetPresentDevice()->GetQueryPool();
        for (auto [start, end] : m_Queries)
        {
            if (start != nullptr)
                queryPool.Release(start);
            if (end != nullptr)
                queryPool.Release(end);
        }
        m_Queries.clear();
    }

    void VulkanTimerQuery::Begin(const Ref<CommandBuffer>& cb)
    {
        VulkanQueryPool& queryPool = gVulkanRenderAPI().GetPresentDevice()->GetQueryPool();

        for (auto [start, end] : m_Queries)
        {
            if (start != nullptr)
                queryPool.Release(start);
            if (end != nullptr)
                queryPool.Release(end);
        }

        m_Queries.clear();
        m_QueryEndCalled = false;
        m_TimeDelta = 0.0f;

        VulkanCommandBuffer* vulkanCB;
        if (cb != nullptr)
            vulkanCB = static_cast<VulkanCommandBuffer*>(cb.get());
        else
            vulkanCB - static_cast<VulkanCommandBuffer*>(gVulkanRenderAPI().GetMainCommandBuffer());

        VulkanCmdBuffer* internalBuffer = vulkanCB->GetInternal();
        VulkanQuery* begin = queryPool.BeginTimerQuery(internalBuffer);
        internalBuffer->RegisterQuery(this);
        m_Queries.push_back(std::make_pair(begin, nullptr));
        SetActive(true);
    }

    void VulkanTimerQuery::End(const Ref<CommandBuffer>& cb)
    {
        if (m_Queries.empty())
        {
            CW_ENGINE_ERROR("VulkanQueryTimer::End called without Begin");
            return;
        }

        m_QueryEndCalled = true;
        m_QueryFinalized = false;
        VulkanCommandBuffer* vulkanCB;
        if (cb != nullptr)
            vulkanCB = static_cast<VulkanCommandBuffer*>(cb.get());
        else
            vulkanCB - static_cast<VulkanCommandBuffer*>(gVulkanRenderAPI().GetMainCommandBuffer());

        VulkanCmdBuffer* internalBuffer = vulkanCB->GetInternal();
        VulkanQueryPool& queryPool = gVulkanRenderAPI().GetPresentDevice()->GetQueryPool();
        VulkanQuery* end = queryPool.BeginTimerQuery(internalBuffer);
        internalBuffer->RegisterQuery(this);
        m_Queries.back().second = end;
    }

    bool VulkanTimerQuery::IsInProgress() const { return !m_Queries.empty() && !m_QueryEndCalled; }

    bool VulkanTimerQuery::IsReady() const
    {
        if (!m_QueryEndCalled)
            return false;
        if (m_QueryFinalized)
            return true;
        uint64_t timerBegin, timerEnd;
        bool ready = true;
        for (auto [start, end] : m_Queries)
        {
            ready &= !start->IsBound() && start->GetResult(timerBegin);
            ready &= !end->IsBound() && end->GetResult(timerEnd);
        }

        return ready;
    }

    float VulkanTimerQuery::GetTimeMs()
    {
        if (!m_QueryFinalized)
        {
            uint64_t totalTimeDiff = 0;
            bool ready = true;
            for (auto [start, end] : m_Queries)
            {
                uint64_t timerBegin = 0, timerEnd = 0;
                ready &= !start->IsBound() && start->GetResult(timerBegin);
                ready &= !end->IsBound() && end->GetResult(timerEnd);
                totalTimeDiff = (timerEnd - timerBegin);
            }

            if (ready)
            {
                m_QueryFinalized = true;

                double timestampToMs =
                  (double)gVulkanRenderAPI().GetPresentDevice()->GetDeviceProperties().limits.timestampPeriod / 1e6;
                m_TimeDelta = (float)((double)totalTimeDiff * timestampToMs);

                VulkanQueryPool& queryPool = gVulkanRenderAPI().GetPresentDevice()->GetQueryPool();
                for (auto [start, end] : m_Queries)
                {
                    if (start != nullptr)
                        queryPool.Release(start);
                    if (end != nullptr)
                        queryPool.Release(end);
                }
            }
        }

        return m_TimeDelta;
    }

    void VulkanTimerQuery::Interrupt(VulkanCmdBuffer* cb)
    {
        CW_ENGINE_ASSERT(!m_Queries.empty() && !m_QueryEndCalled);

        m_QueryEndCalled = true;
        m_QueryFinalized = false;
        VulkanQueryPool& queryPool = gVulkanRenderAPI().GetPresentDevice()->GetQueryPool();
        VulkanQuery* endQuery = queryPool.BeginTimerQuery(cb);
        cb->RegisterQuery(this);
        m_Queries.back().second = endQuery;
    }
} // namespace Crowny
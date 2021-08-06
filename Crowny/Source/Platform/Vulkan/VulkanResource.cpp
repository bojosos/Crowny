#include "cwpch.h"

#include "Platform/Vulkan/VulkanResource.h"

namespace Crowny
{

    VulkanResource::VulkanResource(VulkanResourceManager* owner, bool concurrent)
      : m_Owner(owner), m_QueueFamily(-1), m_State(concurrent ? State::Shared : State::Normal), m_NumUsedHandles(0),
        m_NumBoundHandles(0)
    {
        Cw_ZeroOut(m_ReadUses);
        Cw_ZeroOut(m_WriteUses);
    }

    VulkanResource::~VulkanResource() { CW_ENGINE_ASSERT(m_State == State::Destroyed); }

    void VulkanResource::NotifyBound()
    {
        CW_ENGINE_ASSERT(m_State != State::Destroyed);
        m_NumBoundHandles++;
    }

    void VulkanResource::NotifyUsed(uint32_t globalQueueIdx, uint32_t queueFamily, VulkanAccessFlags useFlags)
    {
        CW_ENGINE_ASSERT(useFlags != VulkanAccessFlagBits::None);
        bool isUsed = m_NumUsedHandles > 0;
        CW_ENGINE_ASSERT(!(isUsed && m_State == State::Normal && m_QueueFamily != queueFamily));
        m_NumUsedHandles++;
        m_QueueFamily = queueFamily;
        if (useFlags.IsSet(VulkanAccessFlagBits::Read))
        {
            CW_ENGINE_ASSERT(m_ReadUses[globalQueueIdx] < 254);
            m_ReadUses[globalQueueIdx]++;
        }
        if (useFlags.IsSet(VulkanAccessFlagBits::Write))
        {
            CW_ENGINE_ASSERT(m_WriteUses[globalQueueIdx] < 254);
            m_WriteUses[globalQueueIdx]++;
        }
    }

    void VulkanResource::NotifyDone(uint32_t globalQueueIdx, VulkanAccessFlags useFlags)
    {
        bool destroy;
        m_NumUsedHandles--;
        m_NumBoundHandles--;
        if (useFlags.IsSet(VulkanAccessFlagBits::Read))
        {
            CW_ENGINE_ASSERT(m_ReadUses[globalQueueIdx] > 0);
            m_ReadUses[globalQueueIdx]--;
        }
        if (useFlags.IsSet(VulkanAccessFlagBits::Write))
        {
            CW_ENGINE_ASSERT(m_WriteUses[globalQueueIdx] > 0);
            m_WriteUses[globalQueueIdx]--;
        }
        bool isBound = m_NumBoundHandles > 0;
        destroy = !isBound && m_State == State::Destroyed;
        if (destroy)
            m_Owner->Destroy(this);
    }

    void VulkanResource::NotifyUnbound()
    {
        bool destroy;
        m_NumBoundHandles--;
        bool isBound = m_NumBoundHandles > 0;
        destroy = !isBound && m_State == State::Destroyed;

        if (destroy)
            m_Owner->Destroy(this);
    }

    uint32_t VulkanResource::GetUseInfo(VulkanAccessFlags useFlags) const
    {
        uint32_t mask = 0;
        if (useFlags.IsSet(VulkanAccessFlagBits::Read))
        {
            for (uint32_t i = 0; i < MAX_UNIQUE_QUEUES; i++)
            {
                if (m_ReadUses[i] > 0)
                    mask |= 1 << i;
            }
        }
        if (useFlags.IsSet(VulkanAccessFlagBits::Write))
        {
            for (uint32_t i = 0; i < MAX_UNIQUE_QUEUES; i++)
            {
                if (m_WriteUses[i] > 0)
                    mask |= 1 << i;
            }
        }

        return mask;
    }

    void VulkanResource::Destroy()
    {
        bool destroy;
        CW_ENGINE_ASSERT(m_State != State::Destroyed);
        m_State = State::Destroyed;
        bool isBound = m_NumBoundHandles > 0;
        destroy = !isBound;
        if (destroy)
            m_Owner->Destroy(this);
    }

    VulkanDevice& VulkanResource::GetDevice() const { return m_Owner->GetDevice(); }

    VulkanResourceManager::VulkanResourceManager(VulkanDevice& device) : m_Device(device) {}

    VulkanResourceManager::~VulkanResourceManager() {}

    void VulkanResourceManager::Destroy(VulkanResource* resource) { delete resource; }

} // namespace Crowny
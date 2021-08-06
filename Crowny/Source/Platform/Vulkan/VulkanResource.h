#pragma once

#include "Platform/Vulkan/VulkanUtils.h"

namespace Crowny
{

    enum class VulkanAccessFlagBits
    {
        None = 1 << 0,
        Read = 1 << 1,
        Write = 1 << 2
    };
    typedef Flags<VulkanAccessFlagBits> VulkanAccessFlags;
    CW_FLAGS_OPERATORS(VulkanAccessFlagBits);

    class VulkanResourceManager;

    class VulkanResource
    {
    public:
        VulkanResource(VulkanResourceManager* owner, bool concurrent);
        virtual ~VulkanResource();

        void NotifyBound();
        void NotifyUsed(uint32_t globalQueueIdx, uint32_t queueFamily, VulkanAccessFlags useFlags);
        virtual void NotifyDone(uint32_t globalQueueIdx, VulkanAccessFlags useFlags);
        virtual void NotifyUnbound();

        void Destroy();
        VulkanDevice& GetDevice() const;
        bool IsUsed() const { return m_NumUsedHandles > 0; }
        bool IsBound() const { return m_NumBoundHandles > 0; }
        bool IsExclusive() const { return m_State != State::Shared; }
        uint32_t GetQueueFamily() const { return m_QueueFamily; }
        uint32_t GetUseCount() const { return m_NumUsedHandles; }
        uint32_t GetBoundCount() const { return m_NumBoundHandles; }
        uint32_t GetUseInfo(VulkanAccessFlags accessFlags) const;

    protected:
        enum class State
        {
            Normal,
            Shared,
            Destroyed
        };

        VulkanResourceManager* m_Owner;
        uint32_t m_QueueFamily;
        State m_State;
        uint8_t m_ReadUses[MAX_UNIQUE_QUEUES];
        uint8_t m_WriteUses[MAX_UNIQUE_QUEUES];

        uint32_t m_NumUsedHandles;
        uint32_t m_NumBoundHandles;
    };

    class VulkanResourceManager
    {
    public:
        VulkanResourceManager(VulkanDevice& device);
        ~VulkanResourceManager();

        template <class Type, class... Args> Type* Create(Args... args)
        {
            return new Type(this, std::forward<Args>(args)...);
        }

        VulkanDevice& GetDevice() const { return m_Device; }

    private:
        friend VulkanResource;

        void Destroy(VulkanResource* resource);
        VulkanDevice& m_Device;
    };

} // namespace Crowny
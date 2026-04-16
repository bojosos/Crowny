#pragma once

#include "Crowny/RenderAPI/AccelerationStructure.h"
#include "Platform/Vulkan/VulkanResource.h"

#include <vulkan/vulkan.h>

namespace Crowny
{
    class VulkanGpuBuffer;

    class VulkanAccelStruct : public VulkanResource
    {
    public:
        VulkanAccelStruct(VulkanResourceManager* owner, VkAccelerationStructureKHR accelStruct);
        ~VulkanAccelStruct();

        VkAccelerationStructureKHR GetHandle() const { return m_AccelStruct; }

    private:
        VkAccelerationStructureKHR m_AccelStruct = VK_NULL_HANDLE;
    };

    class VulkanAccelerationStructure : public AccelerationStructure
    {
    public:
        friend class AccelerationStructure;
        ~VulkanAccelerationStructure();

        virtual void BuildBottomLevel(const Ref<CommandBuffer>& buffer, const AccelerationGeometry* geometry, size_t numGeoms,
                                      AccelerationStructBuildFlags buildFlags) override;
        virtual void BuildTopLevel(const Ref<CommandBuffer>& commandBuffer, AccelerationInstance* instances, size_t numInstances,
                                   AccelerationStructBuildFlags buildFlags) override;

        VulkanAccelStruct* GetAccelStruct() const { return m_AccelStruct; }
        VkDeviceAddress GetDeviceAddress() const { return m_AccelDeviceAddress; }
        VulkanGpuBuffer* GetBuffer() const { return m_Buffer; }

    protected:
        VulkanAccelerationStructure(const Vector<AccelerationGeometry>& topLevelInstances, bool isTopLevel, uint32_t maxTopLevelInstances = 0,
                                    AccelerationStructBuildFlags flags = AccelerationStructBuildBits::None);

    private:
        static VkBuildAccelerationStructureFlagsKHR GetFlags(AccelerationStructBuildFlags buildFlags);

    private:
        VulkanGpuBuffer* m_Buffer = nullptr;
        VulkanAccelStruct* m_AccelStruct = nullptr;
        VkDeviceAddress m_AccelDeviceAddress;
        Vector<VkAccelerationStructureInstanceKHR> m_Instances;
    };

} // namespace Crowny

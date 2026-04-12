#include "cwpch.h"

#include "Platform/Vulkan/VulkanAccelerationStructure.h"
#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanIndexBuffer.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"
#include "Platform/Vulkan/VulkanVertexBuffer.h"

#include <glm/gtc/type_ptr.hpp>

namespace Crowny
{

    VulkanAccelerationStructure::VulkanAccelerationStructure(const Vector<AccelerationGeometry>& topLevelInstances, bool isTopLevel,
                                                             uint32_t maxTopLevelInstances, AccelerationStructBuildFlags flags)
      : AccelerationStructure(topLevelInstances, isTopLevel, maxTopLevelInstances, flags)
    {
        const uint32_t numGeoms = (uint32_t)topLevelInstances.size();
        Vector<VkAccelerationStructureGeometryKHR> geometries(numGeoms);
        Vector<uint32_t> maxPrims(numGeoms);

        for (uint32_t i = 0; i < numGeoms; i++)
        {
            const auto& geom = topLevelInstances[i];
            geometries[i].sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            geometries[i].pNext = nullptr;
            geometries[i].flags = VK_GEOMETRY_OPAQUE_BIT_KHR; // TODO: configurable?

            if (geom.Type == GeometryType::Triangles)
            {
                const auto& tris = geom.GeometryData.Triangles;
                geometries[i].geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
                auto& triangles = geometries[i].geometry.triangles;
                triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
                triangles.pNext = nullptr;
                triangles.vertexFormat = VulkanUtils::GetBufferFormat(tris.VertexFormat);

                VulkanBuffer* vertGpuBuffer = static_cast<VulkanVertexBuffer*>(tris.VertexBuffer.get())->GetBuffer();
                triangles.vertexData.deviceAddress = vertGpuBuffer->GetDeviceAddress();
                triangles.vertexStride = tris.VertexStride;
                triangles.maxVertex = tris.VertexCount;
                if (tris.IndexBuffer != nullptr)
                {
                    triangles.indexType = VulkanUtils::GetIndexType(tris.IndexFormat);
                    VulkanBuffer* indexGpuBuffer = static_cast<VulkanIndexBuffer*>(tris.IndexBuffer.get())->GetBuffer();
                    triangles.indexData.deviceAddress = indexGpuBuffer->GetDeviceAddress();
                }
                else
                {
                    triangles.indexType = VK_INDEX_TYPE_NONE_KHR;
                    triangles.indexData.deviceAddress = 0;
                }
                triangles.transformData.deviceAddress = 0;
                maxPrims[i] = tris.IndexCount / 3;
            }
        }

        VkAccelerationStructureBuildGeometryInfoKHR geometryBuildInfo{};
        geometryBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        geometryBuildInfo.pNext = nullptr;
        geometryBuildInfo.type = isTopLevel ? VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR : VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        geometryBuildInfo.flags = VulkanAccelerationStructure::GetFlags(flags);

        const VkDevice device = gVulkanRenderAPI().GetPresentDevice()->GetLogicalDevice();
        VkAccelerationStructureBuildSizesInfoKHR accelerationStructureSizesInfo{};
        accelerationStructureSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        accelerationStructureSizesInfo.pNext = nullptr;

        uint32_t maxInstanceCount = maxTopLevelInstances;
        const uint32_t* pMaxPrims = isTopLevel ? &maxInstanceCount : maxPrims.data();

        vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &geometryBuildInfo, pMaxPrims,
                                                &accelerationStructureSizesInfo);

        m_Buffer = new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_RAYTRACING, BufferUsage::BU_STATIC_DRAW,
                                       (uint32_t)accelerationStructureSizesInfo.accelerationStructureSize);
        VkAccelerationStructureCreateInfoKHR accelStructCI{};
        accelStructCI.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        accelStructCI.pNext = nullptr;
        accelStructCI.type = isTopLevel ? VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR : VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        accelStructCI.buffer = m_Buffer->GetHandle();
        accelStructCI.size = accelerationStructureSizesInfo.accelerationStructureSize;
        // TODO: Actual reporting here, not just assert
        VkAccelerationStructureKHR accelStructHandle = VK_NULL_HANDLE;
        const VkResult result = vkCreateAccelerationStructureKHR(device, &accelStructCI, gVulkanAllocator, &accelStructHandle);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo;
        accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        accelerationDeviceAddressInfo.pNext = nullptr;
        accelerationDeviceAddressInfo.accelerationStructure = accelStructHandle;
        m_AccelDeviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &accelerationDeviceAddressInfo);

        VulkanResourceManager& owner = gVulkanRenderAPI().GetPresentDevice()->GetResourceManager();
        m_AccelStruct = owner.Create<VulkanAccelStruct>(accelStructHandle);
    }

    VulkanAccelerationStructure::~VulkanAccelerationStructure()
    {
        delete m_Buffer;
        m_Buffer = nullptr;
        m_AccelStruct->Destroy();
        m_AccelStruct = nullptr;
    }

    VkBuildAccelerationStructureFlagsKHR VulkanAccelerationStructure::GetFlags(AccelerationStructBuildFlags buildFlags)
    {
        VkBuildAccelerationStructureFlagsKHR flags = 0;
        if (buildFlags.IsSet(AccelerationStructBuildBits::AllowUpdate))
            flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
        if (buildFlags.IsSet(AccelerationStructBuildBits::PreferFastBuild))
            flags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
        if (buildFlags.IsSet(AccelerationStructBuildBits::PreferFastTrace))
            flags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        return flags;
    }

    void VulkanAccelerationStructure::BuildBottomLevel(const Ref<CommandBuffer>& commandBuffer, const AccelerationGeometry* geometry, size_t numGeoms,
                                                       AccelerationStructBuildFlags buildFlags)
    {
        VulkanCmdBuffer* buffer = static_cast<VulkanCommandBuffer*>(commandBuffer.get())->GetInternal();
        const bool doUpdate = buildFlags.IsSet(AccelerationStructBuildBits::DoUpdate);

        const uint32_t uintNumGeoms = (uint32_t)numGeoms;
        Vector<VkAccelerationStructureGeometryKHR> geometries(uintNumGeoms);
        Vector<uint32_t> maxPrims(uintNumGeoms);
        Vector<VkAccelerationStructureBuildRangeInfoKHR> buildRanges(uintNumGeoms);

        for (uint32_t i = 0; i < uintNumGeoms; i++)
        {
            const auto& geom = geometry[i];
            geometries[i].sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            geometries[i].pNext = nullptr;
            geometries[i].flags = VK_GEOMETRY_OPAQUE_BIT_KHR; // TODO: Configurable

            switch (geom.Type)
            {
            case (GeometryType::Triangles): {
                const auto& tris = geom.GeometryData.Triangles;
                geometries[i].geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
                auto& triangles = geometries[i].geometry.triangles;
                triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
                triangles.pNext = nullptr;
                triangles.vertexFormat = VulkanUtils::GetBufferFormat(tris.VertexFormat);

                VulkanBuffer* vertGpuBuffer = static_cast<VulkanVertexBuffer*>(tris.VertexBuffer.get())->GetBuffer();
                triangles.vertexData.deviceAddress = vertGpuBuffer->GetDeviceAddress();
                triangles.vertexStride = tris.VertexStride;
                triangles.maxVertex = tris.VertexCount;
                if (tris.IndexBuffer != nullptr)
                {
                    triangles.indexType = VulkanUtils::GetIndexType(tris.IndexFormat);
                    VulkanBuffer* indexGpuBuffer = static_cast<VulkanIndexBuffer*>(tris.IndexBuffer.get())->GetBuffer();
                    triangles.indexData.deviceAddress = indexGpuBuffer->GetDeviceAddress();
                }
                else
                {
                    triangles.indexType = VK_INDEX_TYPE_NONE_KHR;
                    triangles.indexData.deviceAddress = 0;
                }
                triangles.transformData.deviceAddress = 0;
                maxPrims[i] = tris.IndexCount / 3;
                buildRanges[i].primitiveCount = tris.IndexCount / 3;
                buildRanges[i].primitiveOffset = 0;
                buildRanges[i].firstVertex = 0;
                buildRanges[i].transformOffset = 0;

                buffer->RegisterBuffer(vertGpuBuffer, BufferUseFlagBits::Acceleration, VulkanAccessFlagBits::Read,
                                       VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
                if (tris.IndexBuffer != nullptr)
                {
                    VulkanBuffer* indexGpuBuffer = static_cast<VulkanIndexBuffer*>(tris.IndexBuffer.get())->GetBuffer();
                    buffer->RegisterBuffer(indexGpuBuffer, BufferUseFlagBits::Acceleration, VulkanAccessFlagBits::Read,
                                           VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
                }
                break;
            }
            default:
                CW_ENGINE_ASSERT(false, "Bad programmer");
            }
        }

        VkAccelerationStructureBuildGeometryInfoKHR accelBuildInfo{};
        accelBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        accelBuildInfo.pNext = nullptr;
        accelBuildInfo.flags = GetFlags(buildFlags);
        accelBuildInfo.pGeometries = geometries.data();
        accelBuildInfo.geometryCount = uintNumGeoms;
        accelBuildInfo.mode = doUpdate ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        accelBuildInfo.dstAccelerationStructure = m_AccelStruct->GetHandle();

        if (m_AllowUpdate)
            accelBuildInfo.flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
        if (doUpdate)
            accelBuildInfo.srcAccelerationStructure = m_AccelStruct->GetHandle();
        const VkDevice device = gVulkanRenderAPI().GetPresentDevice()->GetLogicalDevice();

        VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo{};
        buildSizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &accelBuildInfo, maxPrims.data(),
                                                &buildSizeInfo);
        if (buildSizeInfo.accelerationStructureSize > m_Buffer->GetSize())
        {
            CW_ENGINE_ERROR("Bad programmer: Bottom level accel: {} > {}", (uint32_t)buildSizeInfo.accelerationStructureSize,
                            (uint32_t)m_Buffer->GetSize());
            return;
        }
        const size_t scratchBufferSize = doUpdate ? buildSizeInfo.updateScratchSize : buildSizeInfo.buildScratchSize;
        // TODO: Better scratch stuff...
        VulkanGpuBuffer* scratchBuffer =
          new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_RAYTRACING, BufferUsage::BU_STATIC_DRAW, (uint32_t)scratchBufferSize);
        accelBuildInfo.scratchData.deviceAddress = scratchBuffer->GetBuffer()->GetDeviceAddress();
        const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = buildRanges.data();
        vkCmdBuildAccelerationStructuresKHR(buffer->GetHandle(), 1, &accelBuildInfo, &pBuildRangeInfo);
        buffer->RegisterBuffer(m_Buffer->GetBuffer(), BufferUseFlagBits::Acceleration, VulkanAccessFlagBits::Write,
                               VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
        buffer->RegisterResource(m_AccelStruct, VulkanAccessFlagBits::Write);
    }

    void VulkanAccelerationStructure::BuildTopLevel(const Ref<CommandBuffer>& commandBuffer, AccelerationInstance* instances, size_t numInstances,
                                                    AccelerationStructBuildFlags buildFlags)
    {
        VulkanCmdBuffer* buffer = static_cast<VulkanCommandBuffer*>(commandBuffer.get())->GetInternal();
        m_Instances.resize(numInstances);

        for (size_t i = 0; i < numInstances; i++)
        {
            const AccelerationInstance& instance = instances[i];
            VkAccelerationStructureInstanceKHR& vkInstance = m_Instances[i];
            if (instance.BottomLevelAccel != nullptr)
            {
                const VkDeviceAddress deviceAddress = static_cast<VulkanAccelerationStructure*>(instance.BottomLevelAccel)->GetDeviceAddress();
                vkInstance.accelerationStructureReference = deviceAddress;
            }
            else
                vkInstance.accelerationStructureReference = 0;

            vkInstance.instanceCustomIndex = instance.InstanceID;
            vkInstance.instanceShaderBindingTableRecordOffset = instance.InstanceContribToHitGroupIndex;
            vkInstance.flags = 0; // TODO?
            vkInstance.mask = instance.InstanceMask;
            std::memcpy(vkInstance.transform.matrix, glm::value_ptr(instance.Transform), sizeof(glm::mat3x4));
        }

        const uint32_t uploadSize = (uint32_t)(numInstances * sizeof(VkAccelerationStructureInstanceKHR));
        VulkanGpuBuffer* uploadBufferInfo = new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_GENERIC, BufferUsage::BU_STATIC_DRAW, uploadSize);
        uploadBufferInfo->WriteData(0, uploadSize, m_Instances.data(), BWT_DISCARD);

        const bool doUpdate = buildFlags.IsSet(AccelerationStructBuildBits::DoUpdate);
        if (doUpdate)
            CW_ENGINE_ASSERT(m_AllowUpdate && m_Instances.size() == numInstances);

        VkAccelerationStructureBuildRangeInfoKHR buildRanges{};
        buildRanges.primitiveCount = (uint32_t)numInstances;

        VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
        instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        instancesData.pNext = nullptr;
        instancesData.data.deviceAddress = uploadBufferInfo->GetBuffer()->GetDeviceAddress();
        instancesData.arrayOfPointers = false;

        VkAccelerationStructureGeometryKHR vkGeom{};
        vkGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        vkGeom.pNext = nullptr;
        vkGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        vkGeom.geometry.instances = std::move(instancesData);

        VkAccelerationStructureBuildGeometryInfoKHR accelBuildInfo{};
        accelBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        accelBuildInfo.pNext = nullptr;
        accelBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        accelBuildInfo.mode = doUpdate ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        accelBuildInfo.pGeometries = &vkGeom;
        accelBuildInfo.geometryCount = 1;
        accelBuildInfo.flags = GetFlags(buildFlags);
        accelBuildInfo.dstAccelerationStructure = m_AccelStruct->GetHandle();

        if (m_AllowUpdate)
            accelBuildInfo.flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;

        if (doUpdate)
            accelBuildInfo.srcAccelerationStructure = m_AccelStruct->GetHandle();

        const VkDevice device = gVulkanRenderAPI().GetPresentDevice()->GetLogicalDevice();
        VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo{};
        buildSizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        buildSizeInfo.pNext = nullptr;
        const uint32_t vkNumInstances = (uint32_t)numInstances;
        vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &accelBuildInfo, &vkNumInstances,
                                                &buildSizeInfo);
        if (buildSizeInfo.accelerationStructureSize > m_Buffer->GetSize())
        {
            CW_ENGINE_ERROR("Bad programmer: Top level accel: {} > {}", (uint32_t)buildSizeInfo.accelerationStructureSize,
                            (uint32_t)m_Buffer->GetSize());
            return;
        }

        const size_t scratchBufferSize = doUpdate ? buildSizeInfo.updateScratchSize : buildSizeInfo.buildScratchSize;
        VulkanGpuBuffer* uploadBuffer =
          new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_RAYTRACING, BufferUsage::BU_STATIC_DRAW, (uint32_t)scratchBufferSize);
        accelBuildInfo.scratchData.deviceAddress = uploadBuffer->GetBuffer()->GetDeviceAddress();

        const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &buildRanges;
        vkCmdBuildAccelerationStructuresKHR(buffer->GetHandle(), 1, &accelBuildInfo, &pBuildRangeInfo);
        buffer->RegisterBuffer(m_Buffer->GetBuffer(), BufferUseFlagBits::Acceleration, VulkanAccessFlagBits::Write,
                               VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
        buffer->RegisterResource(m_AccelStruct, VulkanAccessFlagBits::Write);
    }

} // namespace Crowny

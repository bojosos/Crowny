#include "cwpch.h"

#include "Platform/Vulkan/VulkanAccelerationStructure.h"
#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanGpuBuffer.h"
#include "Platform/Vulkan/VulkanIndexBuffer.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"
#include "Platform/Vulkan/VulkanVertexBuffer.h"

#include <glm/gtc/type_ptr.hpp>
#include <vulkan/vulkan.h>

namespace Crowny
{

    VulkanAccelStruct::VulkanAccelStruct(VulkanResourceManager* owner, VkAccelerationStructureKHR accelStruct)
      : VulkanResource(owner, false), m_AccelStruct(accelStruct)
    {
    }

    VulkanAccelStruct::~VulkanAccelStruct()
    {
        vkDestroyAccelerationStructureKHR(m_Owner->GetDevice().GetLogicalDevice(), m_AccelStruct, gVulkanAllocator);
    }

    VulkanAccelerationStructure::VulkanAccelerationStructure(const Vector<AccelerationGeometry>& geometry, bool isTopLevel,
                                                             uint32_t maxTopLevelInstances, AccelerationStructBuildFlags flags)
      : AccelerationStructure(geometry, isTopLevel, maxTopLevelInstances, flags)
    {
        Vector<VkAccelerationStructureGeometryKHR> geometries;
        Vector<VkAccelerationStructureTrianglesOpacityMicromapEXT> opacityMaps;
        Vector<uint32_t> maxPrims;

        VkAccelerationStructureBuildGeometryInfoKHR geometryBuildInfo{};
        geometryBuildInfo.pNext = nullptr;
        geometryBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        if (isTopLevel)
        {
            VkAccelerationStructureGeometryKHR& accelGeometry = geometries.emplace_back();
            accelGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            accelGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
            accelGeometry.geometry.instances;
            maxPrims.push_back(maxTopLevelInstances);
            geometryBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        }
        else
        {
            geometries.resize(geometry.size());
            opacityMaps.resize(geometry.size());
            maxPrims.resize(geometry.size());
            for (uint32_t i = 0; i < (uint32_t)geometry.size(); i++)
                VulkanAccelerationStructure::ConvertGeometry(geometry[i], geometries[i], maxPrims[i], nullptr);

            geometryBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        }
        geometryBuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        geometryBuildInfo.pGeometries = geometries.data();
        geometryBuildInfo.geometryCount = (uint32_t)geometries.size();
        geometryBuildInfo.flags = VulkanAccelerationStructure::GetFlags(flags);

        const VkDevice device = gVulkanRenderAPI().GetPresentDevice()->GetLogicalDevice();
        VkAccelerationStructureBuildSizesInfoKHR accelerationStructureSizesInfo{};
        accelerationStructureSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        accelerationStructureSizesInfo.pNext = nullptr;
        vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &geometryBuildInfo, maxPrims.data(),
                                                &accelerationStructureSizesInfo);

        m_Buffer =
          new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_RAYTRACING, BufferUsage::STATIC_DRAW, accelerationStructureSizesInfo.accelerationStructureSize);
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
        VkBuildAccelerationStructureFlagsKHR vkFlags = 0;
        if (buildFlags.IsSet(AccelerationStructBuildBits::AllowUpdate))
            vkFlags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
        if (buildFlags.IsSet(AccelerationStructBuildBits::AllowCompaction))
            vkFlags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
        if (buildFlags.IsSet(AccelerationStructBuildBits::MinimizeMemory))
            vkFlags |= VK_BUILD_ACCELERATION_STRUCTURE_LOW_MEMORY_BIT_KHR;
        if (buildFlags.IsSet(AccelerationStructBuildBits::DoUpdate))
            vkFlags |= VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
        if (buildFlags.IsSet(AccelerationStructBuildBits::PreferFastBuild))
            vkFlags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
        if (buildFlags.IsSet(AccelerationStructBuildBits::PreferFastTrace))
            vkFlags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        return vkFlags;
    }

    void VulkanAccelerationStructure::ConvertGeometry(const AccelerationGeometry& geom, VkAccelerationStructureGeometryKHR& accelGeom,
                                                      uint32_t& maxPrims, VkAccelerationStructureBuildRangeInfoKHR* range)
    {
        if (geom.Type == GeometryType::Triangles)
        {
            const GeometryTriangles& tris = geom.GeometryData.Triangles;
            VkAccelerationStructureGeometryTrianglesDataKHR vkTris{};
            vkTris.pNext = nullptr;
            vkTris.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            switch (tris.IndexFormat)
            {
            case IndexType::Index_16:
                vkTris.indexType = VK_INDEX_TYPE_UINT16;
                break;
            case IndexType::Index_32:
                vkTris.indexType = VK_INDEX_TYPE_UINT32;
                break;
            }
            vkTris.vertexFormat = VulkanUtils::GetBufferFormat(tris.VertexFormat);
            vkTris.vertexData.deviceAddress =
              static_cast<VulkanVertexBuffer*>(tris.VertexBuffer.get())->GetBuffer()->GetDeviceAddress(); //+ tris.VertexOffset;
            vkTris.vertexStride = tris.VertexBuffer->GetLayout()->GetStride();
            vkTris.maxVertex = std::max(1U, tris.VertexCount);
            vkTris.indexData.deviceAddress =
              static_cast<VulkanIndexBuffer*>(tris.IndexBuffer.get())->GetBuffer()->GetDeviceAddress(); // tris.IndexOffset);

            if (geom.UseTransform)
            {
                VkDeviceOrHostAddressConstKHR transformsAddress{};
                transformsAddress.hostAddress = &geom.Transform;
                vkTris.transformData = transformsAddress;
            }

            // TODO: opacity

            maxPrims = tris.IndexCount / 3;
            accelGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            accelGeom.geometry.triangles = std::move(vkTris);
        }
        else
        {
            CW_ENGINE_ASSERT(false);
        }

        if (range)
            range->primitiveCount = maxPrims;
        VkGeometryFlagsKHR geomFlags = 0;
        if ((geom.Flags & GeometryFlags::Opaque) != 0)
            geomFlags |= VK_GEOMETRY_OPAQUE_BIT_KHR;
        if ((geom.Flags & GeometryFlags::NoDuplicateAnyHitInvocation) != 0)
            geomFlags |= VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
        accelGeom.flags = geomFlags;
    }

    void VulkanAccelerationStructure::BuildBottomLevel(const Ref<CommandBuffer>& commandBuffer, const AccelerationGeometry* geometry, size_t numGeoms,
                                                       AccelerationStructBuildFlags buildFlags)
    {
        VulkanCmdBuffer* buffer = static_cast<VulkanCommandBuffer*>(commandBuffer.get())->GetInternal();
        const bool doUpdate = buildFlags.IsSet(AccelerationStructBuildBits::DoUpdate);
        if (doUpdate)
            CW_ENGINE_ASSERT(m_AllowUpdate);

        Vector<VkAccelerationStructureGeometryKHR> geometries(numGeoms);
        Vector<VkAccelerationStructureTrianglesOpacityMicromapEXT> opacities(numGeoms);
        Vector<VkAccelerationStructureBuildRangeInfoKHR> buildRanges(numGeoms);
        Vector<uint32_t> maxPrims(numGeoms);

        for (size_t i = 0; i < numGeoms; i++)
        {
            ConvertGeometry(geometry[i], geometries[i], maxPrims[i], &buildRanges[i]);

            const AccelerationGeometry& geom = geometry[i];
            switch (geom.Type)
            {
            case GeometryType::Triangles: {
                const GeometryTriangles& tris = geom.GeometryData.Triangles;
                if (tris.VertexBuffer)
                {
                    VulkanBuffer* gpuBuffer = static_cast<VulkanVertexBuffer*>(tris.VertexBuffer.get())->GetBuffer();
                    buffer->RegisterBuffer(gpuBuffer, BufferUseFlagBits::Acceleration, VulkanAccessFlagBits::Read,
                                           VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
                }
                if (tris.IndexBuffer)
                {
                    VulkanBuffer* gpuBuffer = static_cast<VulkanIndexBuffer*>(tris.IndexBuffer.get())->GetBuffer();
                    buffer->RegisterBuffer(gpuBuffer, BufferUseFlagBits::Acceleration, VulkanAccessFlagBits::Read,
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
        accelBuildInfo.geometryCount = numGeoms;
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
            CW_ENGINE_ERROR("Bad programmer: Bottom level accel: {} > {}", buildSizeInfo.accelerationStructureSize, m_Buffer->GetSize());
            return;
        }
        const size_t scratchBufferSize = doUpdate ? buildSizeInfo.updateScratchSize : buildSizeInfo.buildScratchSize;
        // TODO: Better scratch stuff...
        VulkanGpuBuffer* scratchBuffer = new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_RAYTRACING, BufferUsage::STATIC_DRAW, scratchBufferSize);
        accelBuildInfo.scratchData.deviceAddress = scratchBuffer->GetBuffer()->GetDeviceAddress();
        const std::array<const VkAccelerationStructureBuildRangeInfoKHR*, 1> buildRangeInfos = { buildRanges.data() };
        // vkCmdBuildAccelerationStructuresKHR(buffer->GetHandle(), 1, buildInfos.data(), buildRangeInfos.data());
        vkCmdBuildAccelerationStructuresKHR(buffer->GetHandle(), 1, &accelBuildInfo, buildRangeInfos.data());
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
            vkInstance.flags = GetFlags(buildFlags);
            vkInstance.mask = instance.InstanceMask;
            std::memcpy(vkInstance.transform.matrix, glm::value_ptr(instance.Transform), sizeof(glm::mat3x4));
        }

        VulkanGpuBuffer* uploadBufferInfo =
          new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_GENERIC, BufferUsage::STATIC_DRAW, numInstances * sizeof(VkAccelerationStructureInstanceKHR));
        uploadBufferInfo->WriteData(0, numInstances * sizeof(VkAccelerationStructureInstanceKHR), m_Instances.data(), BWT_DISCARD);

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
            if (buildSizeInfo.accelerationStructureSize > m_Buffer->GetSize())
            {
                CW_ENGINE_ERROR("Bad programmer: Bottom level accel: {} > {}", buildSizeInfo.accelerationStructureSize, m_Buffer->GetSize());
                return;
            }

        const size_t scratchBufferSize = doUpdate ? buildSizeInfo.updateScratchSize : buildSizeInfo.buildScratchSize;
        // TODO: Alignments....
        VulkanGpuBuffer* uploadBuffer = new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_RAYTRACING, BufferUsage::STATIC_DRAW, scratchBufferSize);
        accelBuildInfo.scratchData.deviceAddress = uploadBuffer->GetBuffer()->GetDeviceAddress();

        const std::array<const VkAccelerationStructureBuildRangeInfoKHR*, 1> buildRangeInfos = { &buildRanges };
        vkCmdBuildAccelerationStructuresKHR(buffer->GetHandle(), 1, &accelBuildInfo, buildRangeInfos.data());
        buffer->RegisterBuffer(m_Buffer->GetBuffer(), BufferUseFlagBits::Acceleration, VulkanAccessFlagBits::Write,
                               VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
        buffer->RegisterResource(m_AccelStruct, VulkanAccessFlagBits::Write);
    }

} // namespace Crowny
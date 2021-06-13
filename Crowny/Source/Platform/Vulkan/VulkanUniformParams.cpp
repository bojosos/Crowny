#include "cwpch.h"

#include "Platform/Vulkan/VulkanUniforms.h"

namespace Crowny
{
    
    void VulkanUniforms::VulkanUniforms()
    {
        uint32_t numUniformBlocks = 1;
        uint32_t numSets = 1;
        
        VkBuffer* buffers = new VkBuffer[numParamBlocks];
        SetData* setData = new SetData[numSets];

        for (uint32_t i = 0; i < numSets; i++)
        {
            uint32_t numBindingsPerSet = 1;
            SetData& setData = m_SetData[j];
            
            setData.WriteSetInfos = writeSetInfos;
            setData.WriteInfos = writeInfos;
            
            VulkanDescriptorLayout* layout = ;
            for (uiint32_t j = 0; j < numBindingsPerSet; j++)
            {
                VkWriteDescriptorSet& writeSetInfo = setData.WriteSetInfos[j];
                writeSetInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeSetInfo.pNext = nullptr;
                writeSetInfo.dstSet = VK_NULL_HANDLE;
                writeSetInfo.dstBinding = ;
                writeSetInfo.dstArrayElement = 0;
                writeSetInfo.descriptorCount = ;
                writeSetInfo.descriptorType = ;

                uint32_t slot = 0;
                VkDescriptorBufferInfo& bufferInfo = setData.WriteInfos[j].Buffer;
                bufferInfo.offset = 0;
                bufferInfo.range = VK_WHOLE_SIZE;
                if (writeSetInfo.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                {
                    VulkanGpuBuffer* buffer = ;
                    bufferInfo.buffer = buffer->GetHandle();
                    uint32_t sequantialIdx = 0;
                    m_UniformBuffers[sequentialIdx] = bufferInfo.buffer;
                }
                writeSetInfo.pBufferInfo = &bufferInfo;
                writeSetInfo.pTexelBufferView = nullptr;
                writeSetInfo.pImageInfo = nullptr;
            }
        }
    }

    void VulkanUniforms::SetParamBlock(uint32_t set, uint32_t slot, const Ref<UniformBufferBlock>& buffer)
    {
        Uniforms::SetParamBlock(set, slot, buffer);
        VulkanUniformBlock* block = static_cast<VulkanUniformBlock*>(buffer.get());
        
        VulkanBuffer* buffer = nullptr;
        if (block != nullptr)
            buffer = block->GetBuffer();
        
        SetData& data = m_SetData[set];
        if (buffer != nullptr)
        {
            VkBuffer vkBuffer = buffer->GetHandle();
            data.WriteInfos[bindingIdx].Buffer.buffer = vkBuffer;
            m_UniformBuffers[sequentialIdx] = vkBuffer;
        }
        else
        {
            CW_ENGINE_ASSERT(false);
        }
        
        m_SetsDirty[set] = true;
    }

    void VulkanUniforms::Prepare(VulkanCommandBuffer& buffer, VkDescriptorSet* sets)
    {
        uint32_t numParamBlocks = 1;
        uint32_t numSets = 1;
        for (uint32_t i = 0; i < numParamBlocks; i++)
        {
            VulkanBuffer* buffer = nullptr;
            if (m_UniformBlockBuffers[i] != nullptr)
                buffer = static_cast<VulkanUniformBufferBlock*>(m_UniformBlockBuffers[i].get())->GetBuffer();
            
            if (buffer == nullptr)
                CW_ENGINE_ASSERT(false);
            
            VkBuffer vkBuffer = buffer->GetHandle();
            if (m_UniformBuffers != vkBuffer)
            {
                m_UniformBuffers[i] = vkBuffer;
                m_SetData[set].WriteInfos[bindingIdx].Buffer.buffer = vkBuffer;
            }
        }
        
        for (uint32_t i = 0; i < numSets; i++)
        {
            SetData& data = m_SetData[i];
            if (!m_SetsDirty[i])
                continue;
            // need to check if it currently used
            if (data.Latetset == nullptr)
            {
                VulkanDescriptorLayout* layout = getLayout();
                data.LatestSet = CreateSet(layout);
                data.Sets.push_back(data.LatestSet);
            }

            data.LatestSet->Write(data.WriteSetInfos, data.NumElements);
            m_SetsDirty[i] = false;
        }
        
        for (uint32_t i = 0; i < numSets; i++)
        {
            VulkanDescriptorSet* set = m_SetData[i].latestSet;
            sets[i] = set->GetHandle();
        }
    }

}
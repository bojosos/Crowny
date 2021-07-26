#include "cwpch.h"

#include "Platform/Vulkan/VulkanUniformParams.h"
#include "Platform/Vulkan/VulkanDescriptorPool.h"
#include "Platform/Vulkan/VulkanUtils.h"
#include "Platform/Vulkan/VulkanRendererAPI.h"
#include "Platform/Vulkan/VulkanGpuBuffer.h"
#include "Platform/Vulkan/VulkanUniformBufferBlock.h"
#include "Platform/Vulkan/VulkanDescriptorPool.h"
#include "Platform/Vulkan/VulkanGpuBufferManager.h"
#include "Platform/Vulkan/VulkanSamplerState.h"

namespace Crowny
{
    VulkanUniformParamInfo::VulkanUniformParamInfo(const UniformParamDesc& desc) : UniformParamInfo(desc), m_Layouts(), m_LayoutInfos()
    {
        VulkanDevice& device = *gVulkanRendererAPI().GetPresentDevice().get();
        
        uint32_t totalNumSlots = 0;
        for (uint32_t i = 0; i < m_NumSets; i++)
            totalNumSlots += m_SetInfos[i].NumSlots;
        
        m_LayoutInfos = new LayoutInfo[m_NumSets];
        VkDescriptorSetLayoutBinding* bindings = new VkDescriptorSetLayoutBinding[m_NumElements];
        UniformResourceType* types = new UniformResourceType[m_NumElements];
        GpuBufferFormat* elementTypes = new GpuBufferFormat[m_NumElements];
        
        m_SetExtraInfos = new SetExtraInfo[m_NumSets];

        if (bindings != nullptr)
            Cw_ZeroOut(bindings, m_NumElements);
        if (types != nullptr)
            Cw_ZeroOut(types, m_NumElements);
        if (elementTypes != nullptr)
            Cw_ZeroOut(elementTypes, m_NumElements);
        
        uint32_t globalIdx = 0;
        for (uint32_t i = 0; i < m_NumSets; i++)
        {
            m_SetExtraInfos[i].SlotIndices = new uint32_t[m_SetInfos[i].NumSlots];
            
            m_LayoutInfos[i].NumBindings = 0;
            m_LayoutInfos[i].Bindings = nullptr;
            m_LayoutInfos[i].Types = nullptr;
            m_LayoutInfos[i].ElementTypes = nullptr;
            
            for (uint32_t j = 0; j < m_SetInfos[i].NumSlots; j++)
            {
                if (m_SetExtraInfos[i].SlotIndices[j] == (uint32_t)-1)
                {
                    m_SetExtraInfos[i].SlotIndices[j] = (uint32_t)-1;
                    continue;
                }
                VkDescriptorSetLayoutBinding& binding = bindings[globalIdx];
                binding.binding = j;
                m_SetExtraInfos[i].SlotIndices[j] = globalIdx;
                m_LayoutInfos[i].NumBindings++;
                globalIdx++;
            }
        }
        
        uint32_t offset = 0;
        for (uint32_t i = 0; i < m_NumSets; i++)
        {
            m_LayoutInfos[i].Bindings = &bindings[offset];
            m_LayoutInfos[i].Types = &types[offset];
            m_LayoutInfos[i].ElementTypes = &elementTypes[offset];
            offset += m_LayoutInfos[i].NumBindings;
        }
        
        VkShaderStageFlags stageFlagsLookup[6];
        stageFlagsLookup[VERTEX_SHADER] = VK_SHADER_STAGE_VERTEX_BIT;
        stageFlagsLookup[HULL_SHADER] = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        stageFlagsLookup[DOMAIN_SHADER] = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        stageFlagsLookup[GEOMETRY_SHADER] = VK_SHADER_STAGE_GEOMETRY_BIT;
        stageFlagsLookup[FRAGMENT_SHADER] = VK_SHADER_STAGE_FRAGMENT_BIT;
        stageFlagsLookup[COMPUTE_SHADER] = VK_SHADER_STAGE_COMPUTE_BIT;

        for (uint32_t i = 0; i < m_ParamDescs.size(); i++)
        {
            const Ref<UniformDesc>& paramDesc = m_ParamDescs[i];
            if (paramDesc == nullptr)
                continue;

            auto setupBlockBindings = [&](auto& params, VkDescriptorType descType)
            {
                for(auto& entry : params)
                {
                    uint32_t bindingIdx = GetBindingIdx(entry.second.Set, entry.second.Slot);
                    VkDescriptorSetLayoutBinding& binding = bindings[bindingIdx];
                    binding.stageFlags |= stageFlagsLookup[i];
                    binding.descriptorType = descType;
                    binding.descriptorCount = 1;
                }
            };
            
            auto setupBindings = [&](auto& params, VkDescriptorType descType)
            {
                for (auto& entry : params)
                {
                    uint32_t bindingIdx = GetBindingIdx(entry.second.Set, entry.second.Slot);
                    VkDescriptorSetLayoutBinding& binding = bindings[bindingIdx];
                    binding.descriptorCount = 1;
                    binding.stageFlags |= stageFlagsLookup[i];
                    binding.descriptorType = descType;

                    types[bindingIdx] = entry.second.Type;
                    elementTypes[bindingIdx] = entry.second.ElementType;
                }
            };

            setupBlockBindings(paramDesc->Uniforms, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            setupBindings(paramDesc->Textures, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
            setupBindings(paramDesc->LoadStoreTextures, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

            for (auto& entry : paramDesc->Samplers)
            {
                uint32_t bindingIdx = GetBindingIdx(entry.second.Set, entry.second.Slot);
                VkDescriptorSetLayoutBinding& binding = bindings[bindingIdx];

                if (binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                else
                {
                    binding.descriptorCount = 1;
                    binding.stageFlags |= stageFlagsLookup[i];
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                    types[bindingIdx] = entry.second.Type;
                    elementTypes[bindingIdx] = entry.second.ElementType;
                }
            }
            /*
            for (auto& entry : paramDesc->Buffers)
            {
                uint32_t bindingidx = GetBindingIdx(entry.second.Set, entry.second.Slot);
                VkDescriptorSetLayoutBinding& binding = bindings[bindingIdx];
                binding.descriptorCount = 1;
                binding.stageFlags |= stageFlagsLookup[i];
                switch(entry.second.type)
                {
                case(BYTE_BUFFER):
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
                    break;
                case(RWBYTE_BUFFER):
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
                    break;
                case(STRUCTURED_BUFFER):
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    break;
                }

                types[bindingIdx] = entry.second.Type;
                elementTypes[bindingIdx] = entry.second.ElementType;
                binding.descriptorType
            }*/
        }
        VulkanDescriptorManager& descManager = device.GetDescriptorManager();
        m_Layouts = new VulkanDescriptorLayout*[m_NumSets];
        for (uint32_t i = 0; i < m_NumSets; i++)
            m_Layouts[i] = descManager.GetLayout(m_LayoutInfos[i].Bindings, m_LayoutInfos[i].NumBindings);
    }
    
    VulkanUniformParams::VulkanUniformParams(const Ref<UniformParamInfo>& params) : UniformParams(params)
    {
        VulkanUniformParamInfo& paramInfo = static_cast<VulkanUniformParamInfo&>(*m_ParamInfo.get());
        
        uint32_t numParamBlocks = paramInfo.GetNumElements(UniformParamInfo::ParamType::ParamBlock);
        CW_ENGINE_INFO("Param blocks {0}: ", numParamBlocks);
        uint32_t numTextures = paramInfo.GetNumElements(UniformParamInfo::ParamType::Texture);
        uint32_t numStorageTextures = paramInfo.GetNumElements(UniformParamInfo::ParamType::LoadStoreTexture);
        uint32_t numBuffers = paramInfo.GetNumElements(UniformParamInfo::ParamType::Buffer);
        uint32_t numSamplers = paramInfo.GetNumElements(UniformParamInfo::ParamType::SamplerState);
        uint32_t numSets = paramInfo.GetNumSets();
        uint32_t numBindings = paramInfo.GetNumElements();
        
        if (numSets == 0)
            return;
        m_SetsDirty = new bool[numSets];
        m_UniformBuffers = new VkBuffer[numParamBlocks];
        m_PerSetData = new PerSetData[numSets];
        const Ref<VulkanDevice>& device = gVulkanRendererAPI().GetPresentDevice();
        VulkanGpuBufferManager& bufferManager = VulkanGpuBufferManager::Get();
        VulkanDescriptorManager& descManager = device->GetDescriptorManager();
        VulkanSamplerState* defaultSampler = static_cast<VulkanSamplerState*>(SamplerState::GetDefault().get());
        VulkanSampler* vkDefaultSampler = defaultSampler->GetSampler();
       
        if (numSets == 0)
            return;

        for (uint32_t i = 0; i < numSets; i++)
        {
            uint32_t numBindingsPerSet = paramInfo.GetNumBindings(i);
            PerSetData& setData = m_PerSetData[i];
            new (&setData.Sets) std::vector<VulkanDescriptorSet*>();

            VkWriteDescriptorSet* writeSetInfos = new VkWriteDescriptorSet[numBindingsPerSet];
            WriteInfo* writeInfos = new WriteInfo[numBindingsPerSet];
            
            setData.WriteSetInfos = writeSetInfos;
            setData.WriteInfos = writeInfos;
            
            VulkanDescriptorLayout* layout = paramInfo.GetLayout(i);
            setData.Count = numBindingsPerSet;
            setData.LatestSet = descManager.CreateSet(layout);
            setData.Sets.push_back(setData.LatestSet);
            VkDescriptorSetLayoutBinding* setBindings = paramInfo.GetBindings(i);

            UniformResourceType* types = paramInfo.GetLayoutTypes(i);
            GpuBufferFormat* formats = paramInfo.GetLayoutElementTypes(i);

            for (uint32_t j = 0; j < numBindingsPerSet; j++)
            {
                VkWriteDescriptorSet& writeSetInfo = setData.WriteSetInfos[j];
                writeSetInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeSetInfo.pNext = nullptr;
                writeSetInfo.dstSet = VK_NULL_HANDLE;
                writeSetInfo.dstBinding = setBindings[j].binding;
                writeSetInfo.dstArrayElement = 0;
                writeSetInfo.descriptorCount = setBindings[j].descriptorCount;
                writeSetInfo.descriptorType = setBindings[j].descriptorType;

                uint32_t slot = setBindings[j].binding;
                
                if(writeSetInfo.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
                {
                    VkDescriptorImageInfo& imageInfo = setData.WriteInfos[j].Image;
                    imageInfo.sampler = vkDefaultSampler->GetHandle();
                    imageInfo.imageView = VK_NULL_HANDLE;
                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

                    writeSetInfo.pImageInfo = &imageInfo;
                    writeSetInfo.pBufferInfo = nullptr;
                    writeSetInfo.pTexelBufferView = nullptr;
                }
                else
                {
                    bool isImage = writeSetInfo.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || writeSetInfo.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE || writeSetInfo.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                    
                    if (isImage)
                    {
                        //VulkanImage* res = vkTexManager.GetDummy(types[j]);
                        //VkFormat format = VulkanTextureManager::GetDummyViewFormat(types[j]);
                        
                        VkDescriptorImageInfo& imageInfo = setData.WriteInfos[j].Image;
                        
                        bool isLoadStore = writeSetInfo.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                        if (isLoadStore)
                        {
                            imageInfo.sampler = VK_NULL_HANDLE;
                            //imageInfo.imageView = res->GetView(format, false);
                            imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                            uint32_t seqIdx = paramInfo.GetSequentialSlot(UniformParamInfo::ParamType::LoadStoreTexture, j, slot);
                            //m_StorageImages[seqIdx] = res->GetHandle();
                        }
                        else
                        {
                            bool isCombinedImageSampler = writeSetInfo.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                            if (isCombinedImageSampler)
                                imageInfo.sampler = vkDefaultSampler->GetHandle();
                            else
                                imageInfo.sampler = VK_NULL_HANDLE;
                            //imageInfo.imageView = res->GetView(format, false);
                            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                            uint32_t seqIdx = paramInfo.GetSequentialSlot(UniformParamInfo::ParamType::Texture, j, slot);
                            //m_SampledImages[seqIdx] = res->GetHandle();
                        }
                        writeSetInfo.pImageInfo = &imageInfo;
                        writeSetInfo.pBufferInfo = nullptr;
                        writeSetInfo.pTexelBufferView = nullptr;
                    }
                    else
                    {
                        bool useView = writeSetInfo.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && writeSetInfo.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                        
                        if (!useView)
                        {
                            VkDescriptorBufferInfo& bufferInfo = setData.WriteInfos[j].Buffer;
                            bufferInfo.offset = 0;
                            bufferInfo.range = VK_WHOLE_SIZE;
                            if (writeSetInfo.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                            {
                                VulkanGpuBuffer* buffer = bufferManager.GetDummyUniformBuffer();
                                bufferInfo.buffer = buffer->GetHandle();
                                uint32_t seqIdx = paramInfo.GetSequentialSlot(UniformParamInfo::ParamType::ParamBlock, j, slot);
                                m_UniformBuffers[seqIdx] = bufferInfo.buffer;
                            }
                            else
                            {
                                VulkanGpuBuffer* buffer = bufferManager.GetDummyUniformBuffer();
                                bufferInfo.buffer = buffer->GetHandle();
                                uint32_t seqIdx = paramInfo.GetSequentialSlot(UniformParamInfo::ParamType::Buffer, j, slot);
                                m_Buffers[seqIdx] = bufferInfo.buffer;
                            }
                            
                            writeSetInfo.pBufferInfo = &bufferInfo;
                            writeSetInfo.pTexelBufferView = nullptr;
                            writeSetInfo.pImageInfo = nullptr;
                        }
                        else
                        {/*
                            writeSetInfo.pBufferInfo = nullptr;
                            bool isLoadStore = writeSetInfo.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
                            VulkanBuffer* buffer;
                            if (isLoadStore)
                                buffer = bufferManager.GetDummyStorageBuffer()->GetBuffer();
                            else
                                buffer = bufferManager.GetDummyReadBuffer()->GetBuffer();
                        
                            VkFormat format = VulkanUtils::GetBufferFormat(types[j]);
                            VkBufferView bufferView = buffer->GetView(format);
                            setData.WriteInfos[j].BufferView = bufferView;
                            writeSetInfo.pBufferInfo = nullptr;
                            writeSetInfo.pTexelBufferView = &setData.WriteInfos[j].BufferView;
                            
                            uint32_t seqIdx = paramInfo.GetSequentialSlot(UniformParamInfo::ParamType::Buffer, j, slot);
                            m_Buffers[seqIdx] = buffer->GetHandle();*/
                        }
                        
                        writeSetInfo.pImageInfo = nullptr;
                    }
                }
            }
        }
    }

    VulkanUniformParams::~VulkanUniformParams()
    {
        uint32_t numSets = m_ParamInfo->GetNumSets();
        for (uint32_t i = 0; i < numSets; i++)
        {
            for (auto& entry : m_PerSetData[i].Sets)
                entry->Destroy();

            m_PerSetData[i].Sets.~vector<VulkanDescriptorSet*>();
        }
    }

    void VulkanUniformParams::SetUniformBlockBuffer(uint32_t set, uint32_t slot, const Ref<UniformBufferBlock>& buffer)
    {
        UniformParams::SetUniformBlockBuffer(set, slot, buffer);

        VulkanUniformParamInfo& paramInfo = static_cast<VulkanUniformParamInfo&>(*m_ParamInfo.get());
        VulkanUniformBufferBlock* block = static_cast<VulkanUniformBufferBlock*>(buffer.get());
        uint32_t bindingIdx = paramInfo.GetBindingIdx(set, slot);
        if (bindingIdx == (uint32_t)-1)
        {
            CW_ENGINE_ERROR("Set/Slot is not used by the shader.");
            return;
        }
        uint32_t seqIdx = paramInfo.GetSequentialSlot(UniformParamInfo::ParamType::ParamBlock, set, slot);
        
        VulkanUniformBufferBlock* blockBuffer = static_cast<VulkanUniformBufferBlock*>(buffer.get());
        
        VulkanBuffer* result = nullptr;
        if (block != nullptr)
            result = block->GetBuffer();
        
        PerSetData& data = m_PerSetData[set];
        if (buffer != nullptr)
        {
            VkBuffer vkBuffer = result->GetHandle();
            data.WriteInfos[bindingIdx].Buffer.buffer = vkBuffer;
            m_UniformBuffers[seqIdx] = vkBuffer;
        }
        else
        {
            CW_ENGINE_ASSERT(false);
        }
        
        m_SetsDirty[set] = true;
    }

    void VulkanUniformParams::SetTexture(uint32_t slot, uint32_t set, const Ref<Texture>& texture, const TextureSurface& surface)
    {
        UniformParams::SetTexture(set, slot, texture, surface);

        VulkanUniformParamInfo& paramInfo = static_cast<VulkanUniformParamInfo&>(*m_ParamInfo);
        uint32_t bindingIdx = paramInfo.GetBindingIdx(set, slot);
        if (bindingIdx == (uint32_t)-1)
        {
            CW_ENGINE_ERROR("Set/Slot is not used by the shader.");
            return;
        }

        uint32_t seqIdx = paramInfo.GetSequentialSlot(UniformParamInfo::ParamType::SamplerState, set, slot);
        VulkanTexture* vulkanTexture = static_cast<VulkanTexture*>(texture.get());
        VulkanImage* image;
        if (vulkanTexture != nullptr)
            image = vulkanTexture->GetImage();
        else
            image = nullptr;
        PerSetData& data = m_PerSetData[set];
        if (image != nullptr)
        {
            auto& texProps = texture->GetProperties();
            TextureSurface copy = surface;
            if (surface.NumMipLevels == 0)
                copy.NumMipLevels = texProps.MipLevels + 1;
            if (surface.NumFaces == 0)
                copy.NumFaces = texProps.Faces;

            data.WriteInfos[bindingIdx].Image.imageView = image->GetView(copy, false);
            m_SampledImages[seqIdx] = image->GetHandle();
        }
        else
        {
            CW_ENGINE_ASSERT(false, "Not implemented");
        }

        m_SetsDirty[set] = true;
    }


    void VulkanUniformParams::SetSamplerState(uint32_t slot, uint32_t set, const Ref<SamplerState>& sampler)
    {
        UniformParams::SetSamplerState(set, slot, sampler);

        VulkanUniformParamInfo& paramInfo = static_cast<VulkanUniformParamInfo&>(*m_ParamInfo);
        uint32_t bindingIdx = paramInfo.GetBindingIdx(set, slot);
        if (bindingIdx == (uint32_t)-1)
        {
            CW_ENGINE_ERROR("Set/Slot is not used by the shader.");
            return;
        }

        uint32_t seqIdx = paramInfo.GetSequentialSlot(UniformParamInfo::ParamType::SamplerState, set, slot);

        VulkanSamplerState* vulkanSampler = static_cast<VulkanSamplerState*>(sampler.get());
        PerSetData& data = m_PerSetData[set];
        VulkanSampler* resource;
        if (vulkanSampler != nullptr)
            resource = vulkanSampler->GetSampler();
        else
            resource = nullptr;
        if (resource != nullptr)
        {
            VkSampler vkSampler = resource->GetHandle();
            data.WriteInfos[bindingIdx].Image.sampler = vkSampler;
            m_Samplers[seqIdx] = vkSampler;
        }
        else
        {
            VulkanSamplerState* defaultSampler = static_cast<VulkanSamplerState*>(SamplerState::GetDefault().get());
            VkSampler vkSampler = defaultSampler->GetSampler()->GetHandle();
            data.WriteInfos[bindingIdx].Image.sampler = vkSampler;
            m_Samplers[seqIdx] = 0;
        }

        m_SetsDirty[set] = true;
    }


    void VulkanUniformParams::Prepare(VulkanCmdBuffer& buffer, VkDescriptorSet* sets)
    {
        VulkanUniformParamInfo& paramInfo = static_cast<VulkanUniformParamInfo&>(*m_ParamInfo.get());
        
        uint32_t numParamBlocks = paramInfo.GetNumElements(UniformParamInfo::ParamType::ParamBlock);
        uint32_t numTextures = paramInfo.GetNumElements(UniformParamInfo::ParamType::Texture);
        uint32_t numSamplers = paramInfo.GetNumElements(UniformParamInfo::ParamType::SamplerState);
        uint32_t numSets = paramInfo.GetNumSets();

        for (uint32_t i = 0; i < numParamBlocks; i++)
        {
            VulkanBuffer* res = nullptr;
            if (m_BufferBlocks[i] != nullptr)
                res = static_cast<VulkanUniformBufferBlock*>(m_BufferBlocks[i].get())->GetBuffer();
            
            if (res == nullptr)
            {
                res = VulkanGpuBufferManager::Get().GetDummyUniformBuffer()->GetBuffer();
                if (res == nullptr)
                    continue;
            }
            
            uint32_t slot, set;
            m_ParamInfo->GetBinding(UniformParamInfo::ParamType::ParamBlock, i, set, slot);
            uint32_t bindIdx = paramInfo.GetBindingIdx(set, slot);
            VkDescriptorSetLayoutBinding* bindingsPerSet = paramInfo.GetBindings(set);
            VkPipelineStageFlags stages = VulkanUtils::ShaderToPipelineStage(bindingsPerSet[bindIdx].stageFlags);
            
            buffer.RegisterBuffer(res, BufferUseFlagBits::Uniform, VulkanAccessFlagBits::Read, stages);
            CW_ENGINE_ASSERT(m_UniformBuffers[i] != VK_NULL_HANDLE);
            VkBuffer vkBuffer = res->GetHandle();
            if (m_UniformBuffers[i] != vkBuffer)
            {
                m_UniformBuffers[i] = vkBuffer;
                m_PerSetData[set].WriteInfos[bindIdx].Buffer.buffer = vkBuffer;
                m_SetsDirty[set] = true;
            }
        }
        
        VulkanDevice& device = *gVulkanRendererAPI().GetPresentDevice().get();
        VulkanDescriptorManager& descManager = device.GetDescriptorManager();
        
        for (uint32_t i = 0; i < numSamplers; i++)
        {
            if (m_SamplerStates[i] == nullptr)
                continue;
            VulkanSamplerState* state = static_cast<VulkanSamplerState*>(m_SamplerStates[i].get());
            VulkanSampler* sampler = state->GetSampler();
            if (sampler == nullptr)
                continue;
            buffer.RegisterResource(sampler, VulkanAccessFlagBits::Read);

            CW_ENGINE_ASSERT(m_Samplers[i] != VK_NULL_HANDLE);
            VkSampler vkSampler = sampler->GetHandle();
            if (m_Samplers[i] != vkSampler)
            {
                m_Samplers[i] = vkSampler;
                uint32_t set, slot;
                m_ParamInfo->GetBinding(UniformParamInfo::ParamType::SamplerState, i, set, slot);
                uint32_t bindingIdx = paramInfo.GetBindingIdx(set, slot);
                m_PerSetData[set].WriteInfos[bindingIdx].Image.sampler = vkSampler;
                m_SetsDirty[set] = true;
            }
        }

        for (uint32_t i = 0; i < numTextures; i++)
        {
            uint32_t set, slot;
            m_ParamInfo->GetBinding(UniformParamInfo::ParamType::Texture, i, set, slot);
            uint32_t bindingIdx = paramInfo.GetBindingIdx(set, slot);
            
            VulkanImage* image = nullptr;
            VkImageLayout layout;
            if (m_SampledTextureData[i].Texture != nullptr)
            {
                VulkanTexture* texture = static_cast<VulkanTexture*>(m_SampledTextureData[i].Texture.get());
                image = texture->GetImage();

                const TextureParameters& params = texture->GetProperties();
                if (params.Usage & TextureUsage::TEXTURE_DYNAMIC)
                    layout = VK_IMAGE_LAYOUT_GENERAL;
                else
                    layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
            
            if (image == nullptr)
            {
                CW_ENGINE_ASSERT(false, "Broken");
            }
            
            const TextureSurface& surface = m_SampledTextureData[i].Surface;
            VkImageSubresourceRange range = image->GetRange(surface);
            VkDescriptorSetLayoutBinding* perSetBindings = paramInfo.GetBindings(set);
            VkPipelineStageFlags stages = VulkanUtils::ShaderToPipelineStage(perSetBindings[bindingIdx].stageFlags);
            buffer.RegisterImageShader(image, range, layout, VulkanAccessFlagBits::Read, stages);
            layout = buffer.GetCurrentLayout(image, range, true);
            CW_ENGINE_ASSERT(m_SampledImages[i] != VK_NULL_HANDLE);
            VkDescriptorImageInfo& imgInfo = m_PerSetData[set].WriteInfos[bindingIdx].Image;

            VkImage vkImage = image->GetHandle();
            if (m_SampledImages[i] != vkImage)
            {
                m_SampledImages[i] = vkImage;
                VkImageView view;
                if (m_SampledTextureData[i].Texture)
                    view = image->GetView(surface, false);
                else
                {
                    CW_ENGINE_ASSERT(false, "Why here?");
                    //GpuBufferFormat* elementTypes = paramInfo.GetLayoutElementTypes(set);
                    //view = image->GetView(VulkanTextureManager::GetDummyViewFormat(elementTypes[bindingIdx]));
                }

                imgInfo.imageView = view;
                m_SetsDirty[set] = true;
            }

            if (imgInfo.imageLayout != layout)
            {
                imgInfo.imageLayout = layout;
                m_SetsDirty[set] = true;
            }
        }
        
        for (uint32_t i = 0; i < numSets; i++)
        {
            PerSetData& data = m_PerSetData[i];
            if (!m_SetsDirty[i])
                continue;
            
            if (data.LatestSet->IsBound())
            {
                data.LatestSet = nullptr;
                for (auto& entry : data.Sets)
                {
                    if (!entry->IsBound())
                    {
                        data.LatestSet = entry;
                        break;
                    }
                }

                if (data.LatestSet == nullptr)
                {
                    VulkanDescriptorLayout* layout = paramInfo.GetLayout(i);
                    data.LatestSet = descManager.CreateSet(layout);
                    data.Sets.push_back(data.LatestSet);
                }
            }
            data.LatestSet->Write(data.WriteSetInfos, data.Count);
            m_SetsDirty[i] = false;
        }
        
        for (uint32_t i = 0; i < numSets; i++)
        {
            VulkanDescriptorSet* set = m_PerSetData[i].LatestSet;
            buffer.RegisterResource(set, VulkanAccessFlagBits::Read);
            sets[i] = set->GetHandle();
        }
    }

}
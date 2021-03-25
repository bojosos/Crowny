#pragma once

#include "Platform/Vulkan/VulkanUtils.h"

#include "Platform/Vulkan/VulkanDevice.h"

#include "Crowny/Renderer/GraphicsPipeline.h"

namespace Crowny
{
    
    class VulkanPipeline
    {
    public:
        VulkanPipeline(VkDevice* device, VkPipeline pipeline);
        ~VulkanPipeline();
        
        VkPipeline GetHandle() const { return m_Pipeline; }
        
    private:
        VkPipeline m_Pipeline;
    };
    
    class VulkanGraphicsPipeline : public GraphicsPipeline
    {
    public:
        VulkanGraphicsPipeline(const PipelineStateDesc& desc);
        ~VulkanGraphicsPipeline();
        
        VulkanPipeline* CreatePipeline(VulkanRenderPass* renderPass, const BufferLayout& layout, DrawMode drawMode)
        
    private:
        VulkanPipeline* m_Pipeline;
        VkDevice m_Device;

        VkPipelineShaderStageCreateInfo m_ShaderStageInfos[5];
        VkPipelineRasterizationStateCreateInfo m_RasterizationInfo;
        VkPipelineColorBlendAttachmentState m_BlendAttachmentState;
        VkPipelineColorBlendStateCreateInfo m_ColorBlendStateInfo;
        VkPipelineInputAssemblyStateCreateInfo m_InputAssemblyInfo;
        VkPipelineViewportStateCreateInfo m_ViewportInfo;
        VkPipelineMultisampleStateCreateInfo m_MultiSampleInfo;
        VkPipelineDepthStencilStateCreateInfo m_DepthStencilInfo;
        VkGraphicsPipelineCreateInfo m_PipelineInfo;
    };

    class VulkanComputePipeline : public ComputePipeline
    {
    public:
        VulkanComputePipeline(const Ref<Shader>& shader);
        ~VulkanComputePipeline();
        
        VulkanPipeline* GetPipeline() const { return m_Pipeline; };
        VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
    private:
        Ref<VulkanShader> m_Shader;
        VulkanDevice* m_Device;
        VulkanPipeline* m_Pipeline;
        VkPipelineLayout m_PipelineLayout;
    };
    
}
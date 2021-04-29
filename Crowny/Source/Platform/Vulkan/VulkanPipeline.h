#pragma once

#include "Platform/Vulkan/VulkanUtils.h"
#include "Crowny/Renderer/GraphicsPipeline.h"

#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanRenderPass.h"
#include "Platform/Vulkan/VulkanVertexBuffer.h"
#include "Platform/Vulkan/VulkanPipeline.h"
#include "Platform/Vulkan/VulkanShader.h"

namespace Crowny
{
    class VulkanRenderPass;

    class VulkanPipeline
    {
    public:
        VulkanPipeline(VkDevice device, VkPipeline pipeline);
        ~VulkanPipeline();

        VkPipeline GetHandle() const { return m_Pipeline; }

    private:
        VkDevice m_Device;
        VkPipeline m_Pipeline;
    };
    
    class VulkanGraphicsPipeline : public GraphicsPipeline
    {
    public:
        VulkanGraphicsPipeline(const PipelineStateDesc& desc, VulkanVertexBuffer* vertexBuffer);
        ~VulkanGraphicsPipeline();
        
        VulkanPipeline* GetPipeline(VulkanRenderPass* renderPass, DrawMode drawMode);
        VulkanPipeline* CreatePipeline(VulkanRenderPass* renderPass, DrawMode drawMode);
        
    private:
        VkPipelineLayout m_PipelineLayout;
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

    public:
        struct GpuPipelineKey
        {
            GpuPipelineKey(uint32_t renderpass, DrawMode drawMode);

            struct HashFunction
            {
                size_t operator()(const GpuPipelineKey& key) const;
            };

            struct EqualFunction
            {
                bool operator()(const GpuPipelineKey& lhs, const GpuPipelineKey& rhs) const;
            };

            uint32_t FramebufferId;
            DrawMode DrawOp;
        };
        
        std::unordered_map<GpuPipelineKey, VulkanPipeline*, GpuPipelineKey::HashFunction, GpuPipelineKey::EqualFunction> m_Pipelines;
    };

    class VulkanComputePipeline : public ComputePipeline
    {
    public:
        VulkanComputePipeline(const Ref<Shader>& shader);
        ~VulkanComputePipeline();
        
        VkPipeline GetHandle() const { return m_Pipeline; };
        VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
    private:
        Ref<VulkanShader> m_Shader;
        VkDevice m_Device;
        VkPipeline m_Pipeline;
        VkPipelineLayout m_PipelineLayout;
    };

}

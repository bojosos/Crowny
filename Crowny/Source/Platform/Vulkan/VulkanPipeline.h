#pragma once

#include "Crowny/RenderAPI/Buffer.h"
#include "Crowny/RenderAPI/GraphicsPipeline.h"

#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanPipeline.h"
#include "Platform/Vulkan/VulkanRenderPass.h"
#include "Platform/Vulkan/VulkanResource.h"
#include "Platform/Vulkan/VulkanShader.h"
#include "Platform/Vulkan/VulkanUtils.h"

namespace Crowny
{
    class VulkanRenderPass;
    class VulkanCmdBuffer;

    class VulkanPipeline : public VulkanResource
    {
    public:
        VulkanPipeline(VulkanResourceManager* owner, VkPipeline pipeline);
        ~VulkanPipeline();

        VkPipeline GetHandle() const { return m_Pipeline; }

    private:
        VkDevice m_Device;
        VkPipeline m_Pipeline;
    };

    class VulkanGraphicsPipeline : public GraphicsPipeline
    {
    public:
        VulkanGraphicsPipeline(const PipelineStateDesc& desc, const BufferLayout& layout);
        ~VulkanGraphicsPipeline();

        VulkanPipeline* GetPipeline(VulkanRenderPass* renderPass, DrawMode drawMode);
        VulkanPipeline* CreatePipeline(VulkanRenderPass* renderPass, DrawMode drawMode);
        VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
        void RegisterPipelineResources(VulkanCmdBuffer* cmdBuffer);

    private:
        VkPipelineLayout m_PipelineLayout;

        VkPipelineShaderStageCreateInfo m_ShaderStageInfos[5];
        VkVertexInputBindingDescription m_VertexInput = {};
        Vector<VkVertexInputAttributeDescription> m_Attrs;
        VkPipelineRasterizationStateCreateInfo m_RasterizationInfo = {};
        VkPipelineColorBlendAttachmentState m_BlendAttachmentStates[MAX_FRAMEBUFFER_COLOR_ATTACHMENTS];
        VkPipelineColorBlendStateCreateInfo m_ColorBlendStateInfo = {};
        VkPipelineInputAssemblyStateCreateInfo m_InputAssemblyInfo = {};
        VkPipelineViewportStateCreateInfo m_ViewportInfo = {};
        VkPipelineMultisampleStateCreateInfo m_MultiSampleInfo = {};
        VkPipelineDepthStencilStateCreateInfo m_DepthStencilInfo = {};
        VkGraphicsPipelineCreateInfo m_PipelineInfo = {};
        VkPipelineDynamicStateCreateInfo m_DynamicStateCreateInfo = {};
        VkPipelineVertexInputStateCreateInfo m_VertexInputStateCreateInfo = {};

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

        UnorderedMap<GpuPipelineKey, VulkanPipeline*, GpuPipelineKey::HashFunction, GpuPipelineKey::EqualFunction>
          m_Pipelines;
    };

    class VulkanComputePipeline : public ComputePipeline
    {
    public:
        VulkanComputePipeline(const Ref<Shader>& shader);
        ~VulkanComputePipeline();

        VkPipeline GetHandle() const { return m_Pipeline; };
        VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
        void RegisterPipelineResources(VulkanCmdBuffer* cmdBuffer);

    private:
        Ref<VulkanShader> m_Shader;
        VkPipeline m_Pipeline;
        VkPipelineLayout m_PipelineLayout;
    };

} // namespace Crowny

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
    class VulkanBufferLayout;

    class VulkanPipeline : public VulkanResource
    {
    public:
        VulkanPipeline(VulkanResourceManager* owner, VkPipeline pipeline);
        ~VulkanPipeline();

        VkPipeline GetHandle() const { return m_Pipeline; }

    private:
        std::array<bool, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS> m_ReadOnlyColors;
        bool m_DepthReadOnly = false;
        VkPipeline m_Pipeline;
    };

    class VulkanGraphicsPipeline : public GraphicsPipeline
    {
    public:
        VulkanGraphicsPipeline(const PipelineStateDesc& desc, const BufferLayout& layout);
        ~VulkanGraphicsPipeline();

        VulkanPipeline* GetPipeline(VulkanRenderPass* renderPass, uint32_t readOnlyFlags, DrawMode drawMode, const Ref<VulkanBufferLayout>& vulkanBufferLayout);
        VulkanPipeline* CreatePipeline(VulkanRenderPass* renderPass, uint32_t readOnlyFlags, DrawMode drawMode,
                                       const Ref<VulkanBufferLayout>& vulkanBufferLayout);
        VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
        const Ref<BufferLayout> &GetBufferLayout() const { return m_BufferLayout; }
        void RegisterPipelineResources(VulkanCmdBuffer* cmdBuffer);
        bool IsScissorsEnabled() const { return m_ScissorsEnabled; }

    private:
        bool m_ScissorsEnabled = false;
        VkPipelineLayout m_PipelineLayout;
        Ref<BufferLayout> m_BufferLayout;
        VkPipelineShaderStageCreateInfo m_ShaderStageInfos[5];
        VkPipelineRasterizationStateCreateInfo m_RasterizationInfo = {};
        VkPipelineColorBlendAttachmentState m_BlendAttachmentStates[MAX_FRAMEBUFFER_COLOR_ATTACHMENTS];
        VkPipelineColorBlendStateCreateInfo m_ColorBlendStateInfo = {};
        VkPipelineInputAssemblyStateCreateInfo m_InputAssemblyInfo = {};
        VkPipelineTessellationStateCreateInfo m_TesselationInfo = {};
        VkPipelineViewportStateCreateInfo m_ViewportInfo = {};
        VkPipelineMultisampleStateCreateInfo m_MultiSampleInfo = {};
        VkPipelineDepthStencilStateCreateInfo m_DepthStencilInfo = {};
        VkGraphicsPipelineCreateInfo m_PipelineInfo = {};
        VkPipelineDynamicStateCreateInfo m_DynamicStateCreateInfo = {};
        VkPipelineVertexInputStateCreateInfo m_VertexInputStateCreateInfo = {};

    public:
        struct GpuPipelineKey
        {
            GpuPipelineKey(uint32_t renderPass, uint32_t vertId, uint32_t readOnlyFlags, DrawMode drawMode);

            struct HashFunction
            {
                size_t operator()(const GpuPipelineKey& key) const;
            };

            struct EqualFunction
            {
                bool operator()(const GpuPipelineKey& lhs, const GpuPipelineKey& rhs) const;
            };

            uint32_t FramebufferId;
            uint32_t VertexId;
            uint32_t ReadOnlyFlags;
            DrawMode DrawOp;
        };

        UnorderedMap<GpuPipelineKey, VulkanPipeline*, GpuPipelineKey::HashFunction, GpuPipelineKey::EqualFunction>
          m_Pipelines;
    };

    class VulkanComputePipeline : public ComputePipeline
    {
    public:
        VulkanComputePipeline(const Ref<ShaderStage>& shader);
        ~VulkanComputePipeline();

        VulkanPipeline *GetPipeline() const { return m_Pipeline; };
        VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
        void RegisterPipelineResources(VulkanCmdBuffer* cmdBuffer);

    private:
        Ref<VulkanShader> m_Shader;
        VulkanPipeline* m_Pipeline;
        VkPipelineLayout m_PipelineLayout;
    };

} // namespace Crowny

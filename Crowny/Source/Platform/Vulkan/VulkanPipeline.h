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
        bool IsColorReadOnly(uint32_t idx) const { return m_ReadOnlyColors[idx]; }
        bool IsDepthReadOnly() const { return m_DepthReadOnly; }

    private:
        std::array<bool, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS> m_ReadOnlyColors = { false };
        bool m_DepthReadOnly = false;
        VkPipeline m_Pipeline;
    };

    class VulkanGraphicsPipeline : public GraphicsPipeline
    {
    public:
        friend class GraphicsPipeline;
        ~VulkanGraphicsPipeline();

        VulkanPipeline* GetPipeline(VulkanRenderPass* renderPass, uint32_t readOnlyFlags, DrawMode drawMode,
                                    const Ref<VulkanBufferLayout>& vulkanBufferLayout);
        VulkanPipeline* CreatePipeline(VulkanRenderPass* renderPass, uint32_t readOnlyFlags, DrawMode drawMode,
                                       const Ref<VulkanBufferLayout>& vulkanBufferLayout);
        VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
        const Ref<BufferLayout>& GetBufferLayout() const { return m_BufferLayout; }
        void RegisterPipelineResources(VulkanCmdBuffer* cmdBuffer);
        bool IsScissorsEnabled() const { return m_ScissorsEnabled; }

    protected:
        VulkanGraphicsPipeline(const PipelineStateDesc& desc);

    private:
        bool m_ScissorsEnabled = false;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        Ref<BufferLayout> m_BufferLayout;
        VkPipelineShaderStageCreateInfo m_ShaderStageInfos[GRAPHICS_SHADER_COUNT] = {};
        VkPipelineRasterizationStateCreateInfo m_RasterizationInfo = {};
        VkPipelineColorBlendAttachmentState m_BlendAttachmentStates[MAX_FRAMEBUFFER_COLOR_ATTACHMENTS] = {};
        VkPipelineColorBlendStateCreateInfo m_ColorBlendStateInfo = {};
        VkPipelineInputAssemblyStateCreateInfo m_InputAssemblyInfo = {};
        VkPipelineTessellationStateCreateInfo m_TessellationInfo = {};
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

        UnorderedMap<GpuPipelineKey, VulkanPipeline*, GpuPipelineKey::HashFunction, GpuPipelineKey::EqualFunction> m_Pipelines;
    };

    class VulkanRayTracingPipeline : public RayTracingPipeline
    {
    public:
        friend class RayTracingPipeline;
        ~VulkanRayTracingPipeline();

        VulkanPipeline* GetPipeline() const { return m_Pipeline; }
        VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
        void RegisterPipelineResources(VulkanCmdBuffer* buffer);

    protected:
        VulkanRayTracingPipeline(const RayTracingPipelineDesc& desc);

    private:
        std::array<VkPipelineShaderStageCreateInfo, RAYTRACING_SHADER_COUNT> m_ShaderStageInfos = {};
        std::array<VkRayTracingShaderGroupCreateInfoKHR, RAYTRACING_SHADER_COUNT> m_ShaderGroups = {};
        VulkanPipeline* m_Pipeline;
        VkPipelineLayout m_PipelineLayout;
    };

    class VulkanComputePipeline : public ComputePipeline
    {
    public:
        friend class ComputePipeline;
        ~VulkanComputePipeline();

        VulkanPipeline* GetPipeline() const { return m_Pipeline; };
        VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
        void RegisterPipelineResources(VulkanCmdBuffer* cmdBuffer);

    protected:
        VulkanComputePipeline(const Ref<ShaderStage>& shader);

    private:
        Ref<VulkanShader> m_Shader;
        VulkanPipeline* m_Pipeline;
        VkPipelineLayout m_PipelineLayout;
    };

} // namespace Crowny

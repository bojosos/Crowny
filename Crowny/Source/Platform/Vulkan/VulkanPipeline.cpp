#include "cwpch.h"

#include "Platform/Vulkan/VulkanPipeline.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanDescriptorPool.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"
#include "Platform/Vulkan/VulkanRenderPass.h"
#include "Platform/Vulkan/VulkanShader.h"
#include "Platform/Vulkan/VulkanUniformParamInfo.h"
#include "Platform/Vulkan/VulkanUniformParams.h"
#include "Platform/Vulkan/VulkanVertexBuffer.h"

namespace Crowny
{

    VulkanPipeline::VulkanPipeline(VulkanResourceManager* owner, VkPipeline pipeline) : VulkanResource(owner, true), m_Pipeline(pipeline) {}

    VulkanPipeline::~VulkanPipeline() { vkDestroyPipeline(m_Owner->GetDevice().GetLogicalDevice(), m_Pipeline, gVulkanAllocator); }

    VulkanGraphicsPipeline::GpuPipelineKey::GpuPipelineKey(uint32_t renderPass, uint32_t vertexId, uint32_t readOnlyFlags, DrawMode drawMode)
      : FramebufferId(renderPass), VertexId(vertexId), ReadOnlyFlags(readOnlyFlags), DrawOp(drawMode)
    {
    }

    size_t VulkanGraphicsPipeline::GpuPipelineKey::HashFunction::operator()(const GpuPipelineKey& key) const
    {
        size_t hash = 0;
        HashCombine(hash, key.FramebufferId, key.VertexId, key.ReadOnlyFlags, key.DrawOp);
        return hash;
    }

    bool VulkanGraphicsPipeline::GpuPipelineKey::EqualFunction::operator()(const GpuPipelineKey& lhs, const GpuPipelineKey& rhs) const
    {
        if (lhs.FramebufferId != rhs.FramebufferId)
            return false;
        if (lhs.VertexId != rhs.VertexId)
            return false;
        if (lhs.ReadOnlyFlags != rhs.ReadOnlyFlags)
            return false;
        if (lhs.DrawOp != rhs.DrawOp)
            return false;

        return true;
    }

    static VkPipelineDynamicStateCreateInfo GetDynamicStateCreateInfo()
    {
        static Vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_STENCIL_REFERENCE };

        VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo;
        dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateCreateInfo.pNext = nullptr;
        dynamicStateCreateInfo.flags = 0;
        dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();
        dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        return dynamicStateCreateInfo;
    }

    VulkanGraphicsPipeline::VulkanGraphicsPipeline(const PipelineStateDesc& desc) : GraphicsPipeline(desc)
    {
        m_DynamicStateCreateInfo = GetDynamicStateCreateInfo();

        std::pair<VkShaderStageFlagBits, ShaderStage*> stages[] = {
            { VK_SHADER_STAGE_VERTEX_BIT, m_Data.VertexShader.get() },
            { VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, m_Data.HullShader.get() },
            { VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, m_Data.DomainShader.get() },
            { VK_SHADER_STAGE_GEOMETRY_BIT, m_Data.GeometryShader.get() },
            { VK_SHADER_STAGE_FRAGMENT_BIT, m_Data.FragmentShader.get() },
        };

        uint32_t outputIdx = 0;
        const uint32_t numStages = sizeof(stages) / sizeof(stages[0]);
        for (uint32_t i = 0; i < numStages; i++)
        {
            VulkanShader* shader = static_cast<VulkanShader*>(stages[i].second);
            if (shader == nullptr)
                continue;

            VkPipelineShaderStageCreateInfo& stageCI = m_ShaderStageInfos[outputIdx];
            stageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageCI.pNext = nullptr;
            stageCI.flags = 0;
            stageCI.stage = stages[i].first;
            stageCI.module = VK_NULL_HANDLE;
            stageCI.pName = "main"; // TODO: Entry points
            stageCI.pSpecializationInfo = nullptr;

            outputIdx++;
        }

        const uint32_t usedStages = outputIdx;
        const bool tessellation = m_Data.HullShader != nullptr && m_Data.DomainShader != nullptr;

        m_InputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        m_InputAssemblyInfo.pNext = nullptr;
        m_InputAssemblyInfo.flags = 0;
        m_InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // runtime
        m_InputAssemblyInfo.primitiveRestartEnable = false;

        m_TessellationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
        m_TessellationInfo.pNext = nullptr;
        m_TessellationInfo.flags = 0;
        m_TessellationInfo.patchControlPoints = 3; // runtime

        m_ViewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        m_ViewportInfo.pNext = nullptr;
        m_ViewportInfo.flags = 0;
        m_ViewportInfo.viewportCount = 1;
        m_ViewportInfo.scissorCount = 1;
        m_ViewportInfo.pViewports = nullptr; // dynamic state
        m_ViewportInfo.pScissors = nullptr;  // dynamic state

        const Ref<RasterizerStateDesc> rasterizerState = m_Data.RasterizerState ? m_Data.RasterizerState : RasterizerStateDesc::GetDefault();
        const Ref<BlendStateDesc> blendState = m_Data.BlendState ? m_Data.BlendState : BlendStateDesc::GetDefault();
        const Ref<DepthStencilStateDesc> depthState = m_Data.DepthStencilState ? m_Data.DepthStencilState : DepthStencilStateDesc::GetDefault();

        m_RasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        m_RasterizationInfo.pNext = nullptr;
        m_RasterizationInfo.flags = 0;
        m_RasterizationInfo.depthClampEnable = !rasterizerState->DepthClipEnable;
        m_RasterizationInfo.rasterizerDiscardEnable = VK_FALSE; // TODO: ?????
        m_RasterizationInfo.polygonMode = VulkanUtils::GetPolygonMode(rasterizerState->PolygonDrawMode);
        m_RasterizationInfo.cullMode = VulkanUtils::GetCullMode(rasterizerState->CullMode);
        m_RasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // TODO:
        m_RasterizationInfo.depthBiasEnable = rasterizerState->DepthBias != 0.0f;
        m_RasterizationInfo.depthBiasConstantFactor = rasterizerState->DepthBias;
        m_RasterizationInfo.depthBiasSlopeFactor = rasterizerState->DepthBiasSlope;
        m_RasterizationInfo.depthBiasClamp = m_RasterizationInfo.depthClampEnable ? rasterizerState->DepthBiasClamp : 0.0f;
        m_RasterizationInfo.lineWidth = 1.0f;

        VkStencilOpState stencilFront = {};
        stencilFront.compareOp = VulkanUtils::GetCompareOp(depthState->StencilFrontCompare);
        stencilFront.depthFailOp = VulkanUtils::GetStencilOp(depthState->StencilFrontDepthFailOp);
        stencilFront.passOp = VulkanUtils::GetStencilOp(depthState->StencilFrontPassOp);
        stencilFront.failOp = VulkanUtils::GetStencilOp(depthState->StencilFrontFailOp);
        stencilFront.reference = 0; // runtime
        stencilFront.compareMask = (uint32_t)depthState->StencilReadMask;
        stencilFront.writeMask = (uint32_t)depthState->StencilWriteMask;

        VkStencilOpState stencilBack = {};
        stencilBack.compareOp = VulkanUtils::GetCompareOp(depthState->StencilBackCompare);
        stencilBack.depthFailOp = VulkanUtils::GetStencilOp(depthState->StencilBackDepthFailOp);
        stencilBack.passOp = VulkanUtils::GetStencilOp(depthState->StencilBackPassOp);
        stencilBack.failOp = VulkanUtils::GetStencilOp(depthState->StencilBackFailOp);
        stencilBack.reference = 0; // runtime
        stencilBack.compareMask = (uint32_t)depthState->StencilReadMask;
        stencilBack.writeMask = (uint32_t)depthState->StencilWriteMask;

        for (uint32_t i = 0; i < MAX_FRAMEBUFFER_COLOR_ATTACHMENTS; i++)
        {
            // TODO: Per frame buffer attachment properties.
            m_BlendAttachmentStates[i] = {};
            m_BlendAttachmentStates[i].colorWriteMask =
              VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            m_BlendAttachmentStates[i].blendEnable = blendState->EnableBlending;
            m_BlendAttachmentStates[i].colorBlendOp = VulkanUtils::GetBlendOp(blendState->BlendOp);
            m_BlendAttachmentStates[i].srcColorBlendFactor = VulkanUtils::GetBlendFactor(blendState->SrcBlend);
            m_BlendAttachmentStates[i].dstColorBlendFactor = VulkanUtils::GetBlendFactor(blendState->DstBlend);
            m_BlendAttachmentStates[i].alphaBlendOp = VulkanUtils::GetBlendOp(blendState->BlendOpAlpha);
            m_BlendAttachmentStates[i].srcAlphaBlendFactor = VulkanUtils::GetBlendFactor(blendState->SrcBlendAlpha);
            m_BlendAttachmentStates[i].dstAlphaBlendFactor = VulkanUtils::GetBlendFactor(blendState->DstBlendAlpha);
        }

        m_ColorBlendStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        m_ColorBlendStateInfo.pNext = nullptr;
        m_ColorBlendStateInfo.flags = 0;
        m_ColorBlendStateInfo.logicOpEnable = VK_FALSE;
        m_ColorBlendStateInfo.logicOp = VK_LOGIC_OP_NO_OP;
        m_ColorBlendStateInfo.attachmentCount = 1;
        m_ColorBlendStateInfo.pAttachments = m_BlendAttachmentStates;
        m_ColorBlendStateInfo.blendConstants[0] = 0.0f;
        m_ColorBlendStateInfo.blendConstants[1] = 0.0f;
        m_ColorBlendStateInfo.blendConstants[2] = 0.0f;
        m_ColorBlendStateInfo.blendConstants[3] = 0.0f;

        m_DepthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        m_DepthStencilInfo.pNext = nullptr;
        m_DepthStencilInfo.flags = 0;
        m_DepthStencilInfo.depthBoundsTestEnable = false;
        m_DepthStencilInfo.minDepthBounds = 0.0f;
        m_DepthStencilInfo.maxDepthBounds = 1.0f;
        m_DepthStencilInfo.depthTestEnable = depthState->EnableDepthRead;
        m_DepthStencilInfo.depthWriteEnable = depthState->EnableDepthWrite;
        m_DepthStencilInfo.depthCompareOp = VulkanUtils::GetCompareOp(depthState->DepthCompareFunction);
        m_DepthStencilInfo.front = stencilFront;
        m_DepthStencilInfo.back = stencilBack;
        m_DepthStencilInfo.stencilTestEnable = depthState->EnableStencil;

        m_MultiSampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        m_MultiSampleInfo.pNext = nullptr;
        m_MultiSampleInfo.flags = 0;
        m_MultiSampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // runtime
        m_MultiSampleInfo.sampleShadingEnable = VK_FALSE;               // FSAA
        m_MultiSampleInfo.minSampleShading = 1.0f;
        m_MultiSampleInfo.pSampleMask = nullptr;
        m_MultiSampleInfo.alphaToOneEnable = VK_FALSE;
        m_MultiSampleInfo.alphaToCoverageEnable = blendState->AlphaToCoverage;

        m_PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        m_PipelineInfo.pNext = nullptr;
        m_PipelineInfo.flags = 0;
        m_PipelineInfo.stageCount = usedStages;
        m_PipelineInfo.pStages = m_ShaderStageInfos;
        m_PipelineInfo.pVertexInputState = nullptr; // &m_VertexInputStateCreateInfo; // runtime
        m_PipelineInfo.pInputAssemblyState = &m_InputAssemblyInfo;
        m_PipelineInfo.pTessellationState = tessellation ? &m_TessellationInfo : nullptr;
        m_PipelineInfo.pViewportState = &m_ViewportInfo;
        m_PipelineInfo.pRasterizationState = &m_RasterizationInfo;
        m_PipelineInfo.pMultisampleState = &m_MultiSampleInfo;
        m_PipelineInfo.pDepthStencilState = nullptr; // &m_DepthStencilInfo;  // runtime
        m_PipelineInfo.pColorBlendState = nullptr;   // &m_ColorBlendStateInfo; // runtime
        m_PipelineInfo.pDynamicState = &m_DynamicStateCreateInfo;
        m_PipelineInfo.renderPass = VK_NULL_HANDLE;
        m_PipelineInfo.layout = VK_NULL_HANDLE; // m_PipelineLayout;
        m_PipelineInfo.subpass = 0;
        m_PipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        m_PipelineInfo.basePipelineIndex = -1;

        m_ScissorsEnabled = rasterizerState->ScissorsEnabled;

        if (m_Data.VertexShader != nullptr)
            m_BufferLayout = m_Data.VertexShader->GetBufferLayout();

        VulkanDescriptorManager& descManager = gVulkanRenderAPI().GetPresentDevice()->GetDescriptorManager();
        VulkanUniformParamInfo& paramInfo = static_cast<VulkanUniformParamInfo&>(*m_ParamInfo);
        uint32_t numLayouts = paramInfo.GetNumSets();
        VulkanDescriptorLayout** layouts = new VulkanDescriptorLayout*[numLayouts];
        for (uint32_t i = 0; i < numLayouts; i++)
            layouts[i] = paramInfo.GetLayout(i);
        m_PipelineLayout = descManager.GetPipelineLayout(layouts, numLayouts);
        // delete[] layouts;
    }

    VulkanGraphicsPipeline::~VulkanGraphicsPipeline()
    {
        for (auto& entry : m_Pipelines)
            entry.second->Destroy();
    }

    VulkanPipeline* VulkanGraphicsPipeline::GetPipeline(VulkanRenderPass* renderPass, uint32_t readOnlyFlags, DrawMode drawMode,
                                                        const Ref<VulkanBufferLayout>& layout)
    {
        readOnlyFlags &= ~FBT_COLOR;
        GpuPipelineKey key(renderPass->GetId(), layout->GetId(), readOnlyFlags, drawMode);
        auto iter = m_Pipelines.find(key);
        if (iter != m_Pipelines.end())
            return iter->second;
        CW_ENGINE_INFO("Creating pipeline...");
        VulkanPipeline* result = CreatePipeline(renderPass, readOnlyFlags, drawMode, layout);
        m_Pipelines[key] = result;
        return result;
    }

    void VulkanGraphicsPipeline::RegisterPipelineResources(VulkanCmdBuffer* buffer)
    {
        std::array<VulkanShader*, 5> shaders = {
            static_cast<VulkanShader*>(m_Data.VertexShader.get()),   static_cast<VulkanShader*>(m_Data.HullShader.get()),
            static_cast<VulkanShader*>(m_Data.DomainShader.get()),   static_cast<VulkanShader*>(m_Data.GeometryShader.get()),
            static_cast<VulkanShader*>(m_Data.FragmentShader.get()),
        };

        for (auto& shader : shaders)
        {
            if (shader != nullptr)
            {
                VulkanShaderModule* module = shader->GetShaderModule();
                if (module != nullptr)
                    buffer->RegisterResource(module, VulkanAccessFlagBits::Read);
            }
        }
    }

    VulkanPipeline* VulkanGraphicsPipeline::CreatePipeline(VulkanRenderPass* renderPass, uint32_t readOnlyFlags, DrawMode drawMode,
                                                           const Ref<VulkanBufferLayout>& bufferLayout)
    {
        VulkanDevice& device = *gVulkanRenderAPI().GetPresentDevice().get();
        m_InputAssemblyInfo.topology = VulkanUtils::GetDrawFlags(drawMode);
        m_TessellationInfo.patchControlPoints = 3;
        m_MultiSampleInfo.rasterizationSamples = renderPass->GetSampleFlags();
        m_ColorBlendStateInfo.attachmentCount = renderPass->GetNumColorAttachments();

        Ref<DepthStencilStateDesc> depthState = m_Data.DepthStencilState;
        if (depthState == nullptr)
            depthState = DepthStencilStateDesc::GetDefault();
        const bool enableDepthWrite = depthState->EnableDepthWrite && (readOnlyFlags & FBT_DEPTH) == 0;
        m_DepthStencilInfo.depthWriteEnable = enableDepthWrite;

        VkStencilOp oldFrontPassOp = m_DepthStencilInfo.front.passOp;
        VkStencilOp oldFrontFailOp = m_DepthStencilInfo.front.failOp;
        VkStencilOp oldFrontDepthFailOp = m_DepthStencilInfo.front.depthFailOp;

        VkStencilOp oldBackPassOp = m_DepthStencilInfo.back.passOp;
        VkStencilOp oldBackFailOp = m_DepthStencilInfo.back.failOp;
        VkStencilOp oldBackDepthFailOp = m_DepthStencilInfo.back.depthFailOp;

        // Read only depth stencil so we have to modify it.
        if ((readOnlyFlags & FBT_STENCIL) != 0)
        {
            m_DepthStencilInfo.front.passOp = VK_STENCIL_OP_KEEP;
            m_DepthStencilInfo.front.failOp = VK_STENCIL_OP_KEEP;
            m_DepthStencilInfo.front.depthFailOp = VK_STENCIL_OP_KEEP;

            m_DepthStencilInfo.back.passOp = VK_STENCIL_OP_KEEP;
            m_DepthStencilInfo.back.failOp = VK_STENCIL_OP_KEEP;
            m_DepthStencilInfo.back.depthFailOp = VK_STENCIL_OP_KEEP;
        }
        m_PipelineInfo.renderPass = renderPass->GetVkRenderPass(RT_NONE, RT_NONE, CLEAR_NONE);
        m_PipelineInfo.layout = m_PipelineLayout;
        m_PipelineInfo.pVertexInputState = &bufferLayout->GetVkCreateInfo();

        bool depthReadOnly = false;
        if (renderPass->HasDepthAttachment())
        {
            m_PipelineInfo.pDepthStencilState = &m_DepthStencilInfo;
            depthReadOnly = (readOnlyFlags & FBT_DEPTH) != 0;
        }
        else
        {
            m_PipelineInfo.pDepthStencilState = nullptr;
            depthReadOnly = true;
        }

        std::array<bool, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS> colorReadOnly;
        if (renderPass->GetNumColorAttachments() > 0)
        {
            m_PipelineInfo.pColorBlendState = &m_ColorBlendStateInfo;
            for (uint32_t i = 0; i < MAX_FRAMEBUFFER_COLOR_ATTACHMENTS; i++)
                colorReadOnly[i] = m_BlendAttachmentStates[i].colorWriteMask == 0;
        }
        else
        {
            m_PipelineInfo.pColorBlendState = nullptr;
            for (uint32_t i = 0; i < MAX_FRAMEBUFFER_COLOR_ATTACHMENTS; i++)
                colorReadOnly[i] = true;
        }

        const std::pair<VkShaderStageFlagBits, ShaderStage*> stages[] = { { VK_SHADER_STAGE_VERTEX_BIT, m_Data.VertexShader.get() },
                                                                          { VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, m_Data.HullShader.get() },
                                                                          { VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, m_Data.DomainShader.get() },
                                                                          { VK_SHADER_STAGE_GEOMETRY_BIT, m_Data.GeometryShader.get() },
                                                                          { VK_SHADER_STAGE_FRAGMENT_BIT, m_Data.FragmentShader.get() } };
        uint32_t outputIdx = 0;
        const uint32_t numStages = sizeof(stages) / sizeof(stages[0]);
        for (uint32_t i = 0; i < numStages; i++)
        {
            const VulkanShader* shader = static_cast<const VulkanShader*>(stages[i].second);
            if (shader == nullptr)
                continue;
            VulkanShaderModule* module = shader->GetShaderModule();
            if (module != nullptr)
                m_ShaderStageInfos[outputIdx].module = module->GetHandle();
            else
                m_ShaderStageInfos[outputIdx].module = VK_NULL_HANDLE;
            outputIdx++;
        }

        m_PipelineInfo.layout = m_PipelineLayout;
        VkPipeline pipeline;
        const VkResult result =
          vkCreateGraphicsPipelines(device.GetLogicalDevice(), device.GetPipelineCache(), 1, &m_PipelineInfo, gVulkanAllocator, &pipeline);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        m_DepthStencilInfo.front.passOp = oldFrontPassOp;
        m_DepthStencilInfo.front.failOp = oldFrontFailOp;
        m_DepthStencilInfo.front.depthFailOp = oldFrontDepthFailOp;

        m_DepthStencilInfo.back.passOp = oldBackPassOp;
        m_DepthStencilInfo.back.failOp = oldBackFailOp;
        m_DepthStencilInfo.back.depthFailOp = oldBackDepthFailOp;

        return device.GetResourceManager().Create<VulkanPipeline>(pipeline);
    }

    VulkanRayTracingPipeline::VulkanRayTracingPipeline(const RayTracingPipelineDesc& desc) : RayTracingPipeline(desc)
    {
        std::pair<VkShaderStageFlagBits, ShaderStage*> stages[] = { { VK_SHADER_STAGE_RAYGEN_BIT_KHR, m_Data.RaygenShader.get() },
                                                                    { VK_SHADER_STAGE_MISS_BIT_KHR, m_Data.MissShader.get() },
                                                                    { VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, m_Data.HitShader.get() } };

        uint32_t outputIdx = 0;
        const uint32_t numStages = sizeof(stages) / sizeof(stages[0]);
        for (uint32_t i = 0; i < numStages; i++)
        {
            VulkanShader* shader = static_cast<VulkanShader*>(stages[i].second);
            if (shader == nullptr)
                continue;

            VkPipelineShaderStageCreateInfo& stageCI = m_ShaderStageInfos[outputIdx];
            stageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageCI.pNext = nullptr;
            stageCI.flags = 0;
            stageCI.stage = stages[i].first;
            stageCI.module = shader->GetShaderModule()->GetHandle();
            stageCI.pName = "main"; // TODO: Entry points
            stageCI.pSpecializationInfo = nullptr;

            VkRayTracingShaderGroupCreateInfoKHR& shaderGroupCI = m_ShaderGroups[i];
            shaderGroupCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            const bool closestHit = stages[i].first != VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
            shaderGroupCI.type = closestHit ? VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR : VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            shaderGroupCI.generalShader = closestHit ? VK_SHADER_UNUSED_KHR : i;
            shaderGroupCI.closestHitShader = closestHit ? i : VK_SHADER_UNUSED_KHR;
            shaderGroupCI.anyHitShader = VK_SHADER_UNUSED_KHR;
            shaderGroupCI.intersectionShader = VK_SHADER_UNUSED_KHR;

            outputIdx++;
        }

        VulkanRenderAPI& rapi = gVulkanRenderAPI();
        const VulkanDevice& device = *rapi.GetPresentDevice();
        VulkanDescriptorManager& descriptorManager = device.GetDescriptorManager();
        VulkanResourceManager& resourceManager = device.GetResourceManager();
        VulkanUniformParamInfo* vkParams = static_cast<VulkanUniformParamInfo*>(m_ParamInfo.get());

        uint32_t numLayouts = vkParams->GetNumSets();
        VulkanDescriptorLayout** layouts = new VulkanDescriptorLayout*[numLayouts];
        for (uint32_t i = 0; i < numLayouts; i++)
            layouts[i] = vkParams->GetLayout(i);

        // VkPipelineDynamicStateCreateInfo dynamicState = GetDynamicStateCreateInfo();
        VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCI{};
        rayTracingPipelineCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
        rayTracingPipelineCI.pNext = nullptr;
        rayTracingPipelineCI.stageCount = outputIdx;
        rayTracingPipelineCI.pStages = m_ShaderStageInfos.data();
        rayTracingPipelineCI.groupCount = outputIdx;
        rayTracingPipelineCI.pGroups = m_ShaderGroups.data();
        rayTracingPipelineCI.maxPipelineRayRecursionDepth = 1;
        rayTracingPipelineCI.layout = descriptorManager.GetPipelineLayout(layouts, numLayouts);
        rayTracingPipelineCI.pDynamicState = nullptr; //&dynamicState;
        // delete[] layouts;

        VkPipeline pipeline = VK_NULL_HANDLE;
        const VkResult result = vkCreateRayTracingPipelinesKHR(device.GetLogicalDevice(), VK_NULL_HANDLE, device.GetPipelineCache(), 1,
                                                               &rayTracingPipelineCI, gVulkanAllocator, &pipeline);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        m_Pipeline = resourceManager.Create<VulkanPipeline>(pipeline);
        m_PipelineLayout = rayTracingPipelineCI.layout;
    }

    VulkanRayTracingPipeline::~VulkanRayTracingPipeline() { m_Pipeline->Destroy(); }

    void VulkanRayTracingPipeline::RegisterPipelineResources(VulkanCmdBuffer* buffer)
    {
        std::array<VulkanShader*, 5> shaders = {
            static_cast<VulkanShader*>(m_Data.RaygenShader.get()),
            static_cast<VulkanShader*>(m_Data.MissShader.get()),
            static_cast<VulkanShader*>(m_Data.HitShader.get()),
        };

        for (const VulkanShader* shader : shaders)
        {
            if (shader != nullptr)
            {
                VulkanShaderModule* shaderModule = shader->GetShaderModule();
                if (shaderModule != nullptr)
                    buffer->RegisterResource(shaderModule, VulkanAccessFlagBits::Read);
            }
        }
    }

    VulkanComputePipeline::VulkanComputePipeline(const Ref<ShaderStage>& shader) : ComputePipeline(shader)
    {
        const VulkanShader* vulkanShader = static_cast<VulkanShader*>(m_Shader.get());
        VkPipelineShaderStageCreateInfo stageCI;
        stageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageCI.pNext = nullptr;
        stageCI.flags = 0;
        stageCI.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        // stageCI.pName = vulkanShader->entry;
        stageCI.pName = "main";
        stageCI.pSpecializationInfo = nullptr;

        VkComputePipelineCreateInfo pipelineCI;
        pipelineCI.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineCI.pNext = nullptr;
        pipelineCI.flags = 0;
        pipelineCI.stage = stageCI;
        pipelineCI.basePipelineHandle = VK_NULL_HANDLE;
        pipelineCI.basePipelineIndex = -1;

        VulkanRenderAPI& rapi = gVulkanRenderAPI();
        const VulkanDevice& device = *rapi.GetPresentDevice();
        VulkanDescriptorManager& descriptorManager = device.GetDescriptorManager();
        VulkanResourceManager& resourceManager = device.GetResourceManager();
        VulkanUniformParamInfo* vkParams = static_cast<VulkanUniformParamInfo*>(m_ParamInfo.get());

        uint32_t numLayouts = vkParams->GetNumSets();
        VulkanDescriptorLayout** layouts = new VulkanDescriptorLayout*[numLayouts];
        for (uint32_t i = 0; i < numLayouts; i++)
            layouts[i] = vkParams->GetLayout(i);

        const VulkanShaderModule* module = vulkanShader->GetShaderModule();
        if (module != nullptr)
            pipelineCI.stage.module = module->GetHandle();
        else
            pipelineCI.stage.module = VK_NULL_HANDLE;
        pipelineCI.layout = descriptorManager.GetPipelineLayout(layouts, numLayouts);
        // delete[] layouts;

        VkPipeline pipeline = VK_NULL_HANDLE;
        const VkResult result =
          vkCreateComputePipelines(device.GetLogicalDevice(), device.GetPipelineCache(), 1, &pipelineCI, gVulkanAllocator, &pipeline);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        m_Pipeline = resourceManager.Create<VulkanPipeline>(pipeline);
        m_PipelineLayout = pipelineCI.layout;
    }

    VulkanComputePipeline::~VulkanComputePipeline() { m_Pipeline->Destroy(); }

    void VulkanComputePipeline::RegisterPipelineResources(VulkanCmdBuffer* cmdBuffer)
    {
        VulkanShader* shader = static_cast<VulkanShader*>(m_Shader.get());
        if (shader != nullptr)
        {
            VulkanShaderModule* module = shader->GetShaderModule();
            if (module != nullptr)
                cmdBuffer->RegisterResource(module, VulkanAccessFlagBits::Read);
        }
    }

} // namespace Crowny

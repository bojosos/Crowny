#include "cwpch.h"

#include "Platform/Vulkan/VulkanPipeline.h"

#include "Platform/Vulkan/VulkanShader.h"

namespace Crowny
{
    
    VulkanGraphicsPipeline::VulkanGraphicsPipeline(const PipelineStateDesc& desc)
        : m_Data(desc)
    {
        
        std::vector<VkDynamicState> dynamicStates =
        {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        
        VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
        dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateCreateInfo.pNext = nullptr;
        dynamicStateCreateInfo.flags = 0;
        dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();
        dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        
        std::pair<VkShaderStageFlagBits, Shader*> stages[] =
        {
            { VK_SHADER_STAGE_VERTEX_BIT, m_Data.VertexShader.get() },
            { VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, m_Data.HullShader.get() },
            { VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, m_Data.DomainShader.get() },
            { VK_SHADER_STAGE_GEOMETRY_BIT, m_Data.GeometryShader.get() },
            { VK_SHADER_STAGE_FRAGMENT_BIT, m_Data.FragmentShader.get() },
        };
        
        uint32_t outputIdx = 0;
        uint32_t numStages = sizeof(stages) / sizeof(stages[0]);
        for (uint32_t i = 0; i < 5; i++)
        {
            VulkanShader* shader = static_cast<VulkanShader*>(stages[i].second);
            if (shader == nullptr)
                continue;
            
            VkPipelineShaderStageCreateInfo& stageCreateInfo = m_ShaderStageInfos[i];
            stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageCreateInfo.pNext = nullptr;
            stageCreateInfo.flags = 0;
            stageCreateInfo.stage = stages[i].first;
            stageCreateInfo.module = VK_NULL_HANDLE;
            //stageCreateInfo.pName = shader->GetEntryPoint().c_str(); //TODO: entry point of shaders when compiling!
            stageCreateInfo.pName = "main";
            stageCreateInfo.pSpecializationInfo = nullptr;
            
            outputIdx++;
        }
        
        m_InputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        m_InputAssemblyInfo.pNext = nullptr;
        m_InputAssemblyInfo.flags = 0;
        m_InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        m_InputAssemblyInfo.primitiveRestartEnable = false;
        
        m_RasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        m_RasterizationInfo.pNext = nullptr;
        m_RasterizationInfo.flags = 0;
        m_RasterizationInfo.lineWidth = 1.0f;
        m_RasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
        m_RasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
        m_RasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        m_RasterizationInfo.lineWidth = 1.0f;
        
		m_BlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		m_BlendAttachmentState.blendEnable = VK_FALSE;
        
        m_ColorBlendStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        m_ColorBlendStateInfo.pNext = nullptr;
        m_ColorBlendStateInfo.flags = 0;
        m_ColorBlendStateInfo.logicOp = VK_LOGIC_OP_NO_OP;
        m_ColorBlendStateInfo.logicOpEnable = VK_FALSE;
        m_ColorBlendStateInfo.attachmentCount = 1;
        m_ColorBlendStateInfo.pAttachments = &m_BlendAttachmentState;
        
        m_DepthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        m_DepthStencilInfo.pNext = nullptr;
        m_DepthStencilInfo.flags = 0;
        m_DepthStencilInfo.depthTestEnable = VK_FALSE;
        m_DepthStencilInfo.depthWriteEnable = VK_FALSE;
        m_DepthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        m_DepthStencilInfo.front = m_DepthStencilInfo.back;
        m_DepthStencilInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
        
        m_ViewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        m_ViewportInfo.pNext = nullptr;
        m_ViewportInfo.flags = 0;
        m_ViewportInfo.viewportCount = 1;
        m_ViewportInfo.scissorCount = 1;
        m_ViewportInfo.pViewports = nullptr;
        m_ViewportInfo.pScissors = nullptr;
        
        m_MultiSampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        m_MultiSampleInfo.pNext = nullptr;
        m_MultiSampleInfo.flags = 0;
        m_MultiSampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        m_MultiSampleInfo.sampleShadingEnable = VK_FALSE;
        m_MultiSampleInfo.minSampleShading = 1.0f;
        m_MultiSampleInfo.pSampleMask = nullptr;
        m_MultiSampleInfo.alphaToOneEnable = VK_FALSE;
        
        m_PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        m_PipelineInfo.pNext = nullptr;
        m_PipelineInfo.flags = 0;
        m_PipelineInfo.stageCount = outputIdx;
        m_PipelineInfo.pStages = m_ShaderStageInfos;
        m_PipelineInfo.pVertexInputState = nullptr; // runtime
        m_PipelineInfo.layout = VK_NULL_HANDLE; // runtime, 
        m_PipelineInfo.renderPass = VK_NULL_HANDLE; // runtime
        m_PipelineInfo.pInputAssemblyState = &m_InputAssemblyInfo;
        m_PipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        m_PipelineInfo.pDepthStencilState = &m_DepthStencilInfo; // runtime
        m_PipelineInfo.pColorBlendState = &m_ColorBlendStateInfo; // runtime
        m_PipelineInfo.pViewportState = &m_ViewportInfo;
        m_PipelineInfo.pRasterizationState = &m_RasterizationInfo;
        m_PipelineInfo.pMultisampleState = &m_MultiSampleInfo;
        m_PipelineInfo.subpass = 0;
        m_PipelineInfo.pDynamicState = &dynamicStateCreateInfo;
    }
    
    VulkanPipeline* VulkanGraphicsPipeline::CreatePipeline(VulkanRenderPass* renderPass, const BufferLayout& layout, DrawMode drawMode)
    {
        m_InputAssemblyInfo.topology = VulkanUtility::GetDrawMode(drawMode);
        
        // TODO: depth write
            
        m_PipelineInfo.renderPass = renderPass->GetVkRenderPass();
        m_PipelineInfo.layout = renderPass->GetVkRenderPass();
        m_PipelineInfo.pVertexInputState = VulkanUtil::layout
            
        if (renderPass->HasDepthAttachment())
            m_PipelineInfo.p
        
        VkResult result = vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &m_PipelineInfo, nullptr, &m_Pipeline);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
    }
    
}
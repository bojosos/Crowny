#include "cwpch.h"


#include "Platform/Vulkan/VulkanPipeline.h"

#include "Platform/Vulkan/VulkanRenderPass.h"
#include "Platform/Vulkan/VulkanRendererAPI.h"
#include "Platform/Vulkan/VulkanShader.h"

namespace Crowny
{
    
    VulkanPipeline::VulkanPipeline(VkDevice device, VkPipeline pipeline) : m_Device(device), m_Pipeline(pipeline) { }

    VulkanPipeline::~VulkanPipeline()
    {
        vkDestroyPipeline(m_Device, m_Pipeline, nullptr);
    }

    VulkanGraphicsPipeline::GpuPipelineKey::GpuPipelineKey(uint32_t id, DrawMode drawMode)
        : FramebufferId(id), DrawOp(drawMode) { }

    size_t VulkanGraphicsPipeline::GpuPipelineKey::HashFunction::operator()(const GpuPipelineKey& key) const
    {
        size_t hash = 0;
        HashCombine(hash, key.FramebufferId);
        HashCombine(hash, key.DrawOp);
        return hash;
    }

    bool VulkanGraphicsPipeline::GpuPipelineKey::EqualFunction::operator()(const GpuPipelineKey& lhs, const GpuPipelineKey& rhs) const
    {
        if (lhs.FramebufferId != rhs.FramebufferId)
            return false;
        if (lhs.DrawOp != rhs.DrawOp)
            return false;

        return true;
    }

    VulkanGraphicsPipeline::VulkanGraphicsPipeline(const PipelineStateDesc& desc, VulkanVertexBuffer* vertexBuffer)
    {
        m_Data = desc;
        m_Device = gVulkanRendererAPI().GetPresentDevice()->GetLogicalDevice();
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
        for (uint32_t i = 0; i < numStages; i++)
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
        m_InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // runtime
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
        
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pushConstantRangeCount = 0;

        CW_ENGINE_ASSERT(vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout) == VK_SUCCESS);

        BufferLayout layout = vertexBuffer->GetLayout();
        VkVertexInputBindingDescription vertexInput{};
        vertexInput.binding = 0;
        vertexInput.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        vertexInput.stride = layout.GetStride();
        
        std::vector<VkVertexInputAttributeDescription> attrs(layout.GetElements().size());

        // loc, binding, format, offset
		for (int idx = 0; idx < layout.GetElements().size(); idx++)
		{
            const auto& element = layout.GetElements().at(idx);
            uint32_t offset = static_cast<uint32_t>(element.Offset);
            uint32_t i = static_cast<uint32_t>(idx);
			switch (element.Type)
			{
                case ShaderDataType::Float:  { attrs[i] = { i, 0, VK_FORMAT_R32_SFLOAT, offset }; break; }
				case ShaderDataType::Float2: { attrs[i] = { i, 0, VK_FORMAT_R32G32_SFLOAT, offset }; break; }
				case ShaderDataType::Float3: { attrs[i] = { i, 0, VK_FORMAT_R32G32B32_SFLOAT, offset }; break; }
				case ShaderDataType::Float4: { attrs[i] = { i, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offset }; break; }
                //case ShaderDataType::Bool:   // dk how to do this one
                case ShaderDataType::Mat3:   //attrs[i] = { i, 0, VK_FORMAT_R32_SFLOAT, element.Offset }; 
                case ShaderDataType::Mat4:   { CW_ENGINE_ASSERT(false); break; }// these will probably be sent as 3/4 vectors }
                case ShaderDataType::Int:    { attrs[i] = { i, 0, VK_FORMAT_R32_SINT, offset }; break; }
				case ShaderDataType::Int2:   { attrs[i] = { i, 0, VK_FORMAT_R32G32_SINT, offset }; break; }
				case ShaderDataType::Int3:   { attrs[i] = { i, 0, VK_FORMAT_R32G32B32_SINT, offset }; break; }
				case ShaderDataType::Int4:   { attrs[i] = { i, 0, VK_FORMAT_R32G32B32A32_SINT, offset }; break; }
				default:
					CW_ENGINE_ASSERT(false, "Unknown ShaderDataType!");
			}
		}

        VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
        vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
        vertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexInput;
        vertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
        vertexInputStateCreateInfo.pVertexAttributeDescriptions = attrs.data();
        
        m_PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        m_PipelineInfo.pNext = nullptr;
        m_PipelineInfo.flags = 0;
        m_PipelineInfo.stageCount = outputIdx;
        m_PipelineInfo.pStages = m_ShaderStageInfos;
        m_PipelineInfo.pVertexInputState = &vertexInputStateCreateInfo; // runtime
        m_PipelineInfo.layout = m_PipelineLayout; // runtime, //TODO: fix this, for uniforms
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

    VulkanGraphicsPipeline::~VulkanGraphicsPipeline()
    {
        for (auto& entry : m_Pipelines)
            delete entry.second;
    }

    VulkanPipeline* VulkanGraphicsPipeline::GetPipeline(VulkanRenderPass* renderpass, DrawMode drawMode)
    {
        GpuPipelineKey key(renderpass->GetId(), drawMode);
        auto iter = m_Pipelines.find(key);
        if (iter != m_Pipelines.end())
            return iter->second;

        VulkanPipeline* result = CreatePipeline(renderpass, drawMode);
        m_Pipelines[key] = result;
        return result;
    }

    VulkanPipeline* VulkanGraphicsPipeline::CreatePipeline(VulkanRenderPass* renderpass, DrawMode drawMode)
    {
        m_MultiSampleInfo.rasterizationSamples = renderpass->GetSampleFlags();
        m_ColorBlendStateInfo.attachmentCount = renderpass->GetNumColorAttachments();

        m_PipelineInfo.renderPass = renderpass->GetHandle();
        // TODO: Vertex input, depth stencil, color blending, stages
        m_PipelineInfo.layout = m_PipelineLayout;
        //m_PipelineInfo.pVertexInputState
        VkPipeline pipeline;
        VkResult result = vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &m_PipelineInfo, nullptr, &pipeline);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        
        return new VulkanPipeline(m_Device, pipeline);
    }

    VulkanComputePipeline::VulkanComputePipeline(const Ref<Shader>& shader)
    {

    }

    VulkanComputePipeline::~VulkanComputePipeline()
    {
        
    }
      
}

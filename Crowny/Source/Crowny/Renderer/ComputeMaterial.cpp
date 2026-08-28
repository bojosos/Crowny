#include "cwpch.h"

#include "Crowny/Renderer/ComputeMaterial.h"

#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/Renderer/ShaderVariation.h"

namespace Crowny
{
    bool ComputeMaterial::Initialize(const AssetHandle<Shader>& shader)
    {
        Reset();
        if (!shader)
        {
            m_Error = "Compute shader asset is not loaded";
            return false;
        }

        const Ref<ShaderTechnique>& technique = shader->GetTechnique(ShaderVariation::EMPTY);
        if (!technique)
        {
            m_Error = "Compute shader has no technique";
            return false;
        }
        Ref<ShaderRenderPass> computePass;
        for (const Ref<ShaderRenderPass>& pass : technique->GetRenderPasses())
        {
            if (pass && pass->IsCompute())
            {
                computePass = pass;
                break;
            }
        }
        if (!computePass)
        {
            m_Error = "Shader technique has no compute pass";
            return false;
        }
        if (!computePass->GetComputePipeline())
            computePass->Compile();
        m_Pipeline = computePass->GetComputePipeline();
        if (!m_Pipeline)
        {
            m_Error = "Compute pipeline creation failed";
            return false;
        }

        m_Uniforms = UniformParams::Create(m_Pipeline);
        if (!m_Uniforms)
        {
            m_Error = "Compute uniform resource creation failed";
            m_Pipeline = nullptr;
            return false;
        }
        const Ref<UniformDesc>& desc = m_Pipeline->GetParamInfo()->GetUniformDesc(COMPUTE_SHADER);
        if (desc)
        {
            for (const auto& [name, blockDesc] : desc->Uniforms)
            {
                UniformBlock block;
                block.Size = blockDesc.BlockSize;
                block.Buffer = UniformBufferBlock::Create(block.Size, BufferUsage::BU_DYNAMIC_DRAW);
                m_Uniforms->SetUniformBlockBuffer(blockDesc.Set, blockDesc.Slot, block.Buffer);
                const auto [entry, _] = m_UniformBlocks.emplace(name, std::move(block));
                m_UniformBlocksByBinding.emplace((static_cast<uint64_t>(blockDesc.Set) << 32u) | blockDesc.Slot,
                                                  &entry->second);
            }
        }
        m_Shader = shader;
        return true;
    }

    void ComputeMaterial::Reset()
    {
        m_Shader = {};
        m_Pipeline = nullptr;
        m_Uniforms = nullptr;
        m_UniformBlocks.clear();
        m_UniformBlocksByBinding.clear();
        m_Error.clear();
    }

    bool ComputeMaterial::WriteUniformBlock(StringView name, const void* data, uint32_t size, uint32_t offset)
    {
        const auto block = m_UniformBlocks.find(name);
        if (block == m_UniformBlocks.end() || data == nullptr || offset > block->second.Size ||
            size > block->second.Size - offset)
            return false;
        block->second.Buffer->Write(offset, data, size);
        return true;
    }

    bool ComputeMaterial::WriteUniformBlock(uint32_t set, uint32_t slot, const void* data, uint32_t size,
                                            uint32_t offset)
    {
        const auto block = m_UniformBlocksByBinding.find((static_cast<uint64_t>(set) << 32u) | slot);
        if (block == m_UniformBlocksByBinding.end() || data == nullptr || offset > block->second->Size ||
            size > block->second->Size - offset)
            return false;
        block->second->Buffer->Write(offset, data, size);
        return true;
    }

    bool ComputeMaterial::SetBuffer(StringView name, const Ref<GenericGpuBuffer>& buffer)
    {
        if (!m_Pipeline || !m_Uniforms)
            return false;
        const Ref<UniformDesc>& desc = m_Pipeline->GetParamInfo()->GetUniformDesc(COMPUTE_SHADER);
        if (!desc)
            return false;
        const auto resource = desc->Buffers.find(String(name));
        if (resource == desc->Buffers.end())
            return false;
        m_Uniforms->SetBuffer(resource->second.Set, resource->second.Slot, buffer);
        return true;
    }

    bool ComputeMaterial::SetBuffer(uint32_t set, uint32_t slot, const Ref<GenericGpuBuffer>& buffer)
    {
        if (!m_Uniforms)
            return false;
        m_Uniforms->SetBuffer(set, slot, buffer);
        return true;
    }

    bool ComputeMaterial::SetTexture(StringView name, const Ref<Texture>& texture, const TextureSurface& surface)
    {
        if (!m_Pipeline || !m_Uniforms)
            return false;
        const Ref<UniformDesc>& desc = m_Pipeline->GetParamInfo()->GetUniformDesc(COMPUTE_SHADER);
        if (!desc)
            return false;
        const auto resource = desc->Textures.find(String(name));
        if (resource == desc->Textures.end())
            return false;
        m_Uniforms->SetTexture(resource->second.Set, resource->second.Slot, texture, surface);
        return true;
    }

    bool ComputeMaterial::SetTexture(uint32_t set, uint32_t slot, const Ref<Texture>& texture,
                                     const TextureSurface& surface)
    {
        if (!m_Uniforms)
            return false;
        m_Uniforms->SetTexture(set, slot, texture, surface);
        return true;
    }

    bool ComputeMaterial::SetTextureArray(uint32_t set, uint32_t slot, const Ref<Texture>* textures, uint32_t count,
                                          const TextureSurface* surfaces)
    {
        if (!m_Uniforms)
            return false;
        m_Uniforms->SetTextureArray(set, slot, textures, count, surfaces);
        return true;
    }

    bool ComputeMaterial::SetSamplerState(uint32_t set, uint32_t slot, const Ref<SamplerState>& sampler)
    {
        if (!m_Uniforms)
            return false;
        m_Uniforms->SetSamplerState(set, slot, sampler);
        return true;
    }

    bool ComputeMaterial::SetLoadStoreTexture(StringView name, const Ref<Texture>& texture,
                                              const TextureSurface& surface)
    {
        if (!m_Pipeline || !m_Uniforms)
            return false;
        const Ref<UniformDesc>& desc = m_Pipeline->GetParamInfo()->GetUniformDesc(COMPUTE_SHADER);
        if (!desc)
            return false;
        const auto resource = desc->LoadStoreTextures.find(String(name));
        if (resource == desc->LoadStoreTextures.end())
            return false;
        m_Uniforms->SetLoadStoreTexture(resource->second.Set, resource->second.Slot, texture, surface);
        return true;
    }

    bool ComputeMaterial::SetLoadStoreTexture(uint32_t set, uint32_t slot, const Ref<Texture>& texture,
                                              const TextureSurface& surface)
    {
        if (!m_Uniforms)
            return false;
        m_Uniforms->SetLoadStoreTexture(set, slot, texture, surface);
        return true;
    }

    bool ComputeMaterial::Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ,
                                   const Ref<CommandBuffer>& commandBuffer)
    {
        if (!IsValid() || RenderAPI::TryGet() == nullptr || groupsX == 0 || groupsY == 0 || groupsZ == 0)
            return false;
        for (const auto& [_, block] : m_UniformBlocks)
            block.Buffer->FlushToGpu();
        RenderAPI::TryGet()->SetComputePipeline(m_Pipeline, commandBuffer);
        RenderAPI::TryGet()->SetUniforms(m_Uniforms, commandBuffer);
        RenderAPI::TryGet()->DispatchCompute(groupsX, groupsY, groupsZ, commandBuffer);
        return true;
    }

    bool GraphicsMaterial::Initialize(const AssetHandle<Shader>& shader,
                                      const Ref<BlendStateDesc>& blendStateOverride,
                                      const Ref<DepthStencilStateDesc>& depthStateOverride)
    {
        return Initialize(shader, ShaderVariation::EMPTY, blendStateOverride, depthStateOverride);
    }

    bool GraphicsMaterial::Initialize(const AssetHandle<Shader>& shader, const ShaderVariation& variation,
                                      const Ref<BlendStateDesc>& blendStateOverride,
                                      const Ref<DepthStencilStateDesc>& depthStateOverride)
    {
        Reset();
        if (!shader)
        {
            m_Error = "Graphics shader asset is not loaded";
            return false;
        }
        const Ref<ShaderTechnique>& technique = shader->GetTechnique(variation);
        if (!technique)
        {
            m_Error = "Graphics shader has no technique";
            return false;
        }
        Ref<ShaderRenderPass> graphicsPass;
        for (const Ref<ShaderRenderPass>& pass : technique->GetRenderPasses())
        {
            if (pass && !pass->IsCompute() && !pass->IsRayTrace())
            {
                graphicsPass = pass;
                break;
            }
        }
        if (!graphicsPass)
        {
            m_Error = "Shader technique has no graphics pass";
            return false;
        }
        if (blendStateOverride || depthStateOverride)
        {
            const ShaderRenderPassDesc& pass = graphicsPass->GetPassDesc();
            PipelineStateDesc pipelineDesc;
            if (pass.VertexShader)
                pipelineDesc.VertexShader = ShaderStage::Create(pass.VertexShader);
            if (pass.FragmentShader)
                pipelineDesc.FragmentShader = ShaderStage::Create(pass.FragmentShader);
            if (pass.GeometryShader)
                pipelineDesc.GeometryShader = ShaderStage::Create(pass.GeometryShader);
            if (pass.HullShader)
                pipelineDesc.HullShader = ShaderStage::Create(pass.HullShader);
            if (pass.DomainShader)
                pipelineDesc.DomainShader = ShaderStage::Create(pass.DomainShader);
            pipelineDesc.BlendState = blendStateOverride ? blendStateOverride : pass.BlendState;
            pipelineDesc.DepthStencilState = depthStateOverride ? depthStateOverride : pass.DepthStencilState;
            pipelineDesc.RasterizerState = pass.RasterizationState;
            m_Pipeline = GraphicsPipeline::Create(pipelineDesc);
        }
        else
        {
            if (!graphicsPass->GetGraphicsPipeline())
                graphicsPass->Compile();
            m_Pipeline = graphicsPass->GetGraphicsPipeline();
        }
        if (!m_Pipeline)
        {
            m_Error = "Graphics pipeline creation failed";
            return false;
        }
        m_Uniforms = UniformParams::Create(m_Pipeline);
        if (!m_Uniforms)
        {
            m_Error = "Graphics uniform resource creation failed";
            m_Pipeline = nullptr;
            return false;
        }
        const Ref<UniformParamInfo>& info = m_Pipeline->GetParamInfo();
        for (uint32_t shaderType = 0; shaderType < SHADER_COUNT; shaderType++)
        {
            const Ref<UniformDesc>& desc = info->GetUniformDesc(static_cast<ShaderType>(shaderType));
            if (!desc)
                continue;
            for (const auto& [_, blockDesc] : desc->Uniforms)
            {
                const uint64_t binding = (static_cast<uint64_t>(blockDesc.Set) << 32u) | blockDesc.Slot;
                if (m_UniformBlocks.find(binding) != m_UniformBlocks.end())
                    continue;
                UniformBlock block;
                block.Size = blockDesc.BlockSize;
                block.Buffer = UniformBufferBlock::Create(block.Size, BufferUsage::BU_DYNAMIC_DRAW);
                m_Uniforms->SetUniformBlockBuffer(blockDesc.Set, blockDesc.Slot, block.Buffer);
                m_UniformBlocks.emplace(binding, std::move(block));
            }
        }
        m_Shader = shader;
        return true;
    }

    void GraphicsMaterial::Reset()
    {
        m_Shader = {};
        m_Pipeline = nullptr;
        m_Uniforms = nullptr;
        m_UniformBlocks.clear();
        m_Error.clear();
    }

    bool GraphicsMaterial::WriteUniformBlock(uint32_t set, uint32_t slot, const void* data, uint32_t size,
                                             uint32_t offset)
    {
        const auto block = m_UniformBlocks.find((static_cast<uint64_t>(set) << 32u) | slot);
        if (block == m_UniformBlocks.end() || data == nullptr || offset > block->second.Size ||
            size > block->second.Size - offset)
            return false;
        block->second.Buffer->Write(offset, data, size);
        return true;
    }

    bool GraphicsMaterial::SetBuffer(uint32_t set, uint32_t slot, const Ref<GenericGpuBuffer>& buffer)
    {
        if (!m_Uniforms)
            return false;
        m_Uniforms->SetBuffer(set, slot, buffer);
        return true;
    }

    bool GraphicsMaterial::SetTexture(uint32_t set, uint32_t slot, const Ref<Texture>& texture,
                                      const TextureSurface& surface)
    {
        if (!m_Uniforms)
            return false;
        m_Uniforms->SetTexture(set, slot, texture, surface);
        return true;
    }

    bool GraphicsMaterial::SetTextureArray(uint32_t set, uint32_t slot, const Ref<Texture>* textures,
                                           uint32_t count, const TextureSurface* surfaces)
    {
        if (!m_Uniforms)
            return false;
        m_Uniforms->SetTextureArray(set, slot, textures, count, surfaces);
        return true;
    }

    bool GraphicsMaterial::SetSamplerState(uint32_t set, uint32_t slot, const Ref<SamplerState>& sampler)
    {
        if (!m_Uniforms)
            return false;
        m_Uniforms->SetSamplerState(set, slot, sampler);
        return true;
    }

    bool GraphicsMaterial::Bind(const Ref<CommandBuffer>& commandBuffer)
    {
        if (!IsValid() || RenderAPI::TryGet() == nullptr)
            return false;
        for (const auto& [_, block] : m_UniformBlocks)
            block.Buffer->FlushToGpu();
        RenderAPI::TryGet()->SetGraphicsPipeline(m_Pipeline, commandBuffer);
        RenderAPI::TryGet()->SetUniforms(m_Uniforms, commandBuffer);
        return true;
    }
} // namespace Crowny

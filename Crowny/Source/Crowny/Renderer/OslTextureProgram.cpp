#include "cwpch.h"

#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/Renderer/OslTextureProgram.h"
#include "Crowny/Utils/ShaderCompiler.h"

#include <spirv_cross/spirv_cross.hpp>

namespace Crowny
{
    bool OslTextureProgram::Initialize(const Vector<uint8_t>& validatedSpirv, String& error)
    {
        m_Pipeline = nullptr;
        m_Uniforms = nullptr;
        error.clear();
        if (RenderAPI::TryGet() == nullptr || RenderAPI::GetAPI() != RenderAPI::API::Vulkan)
        {
            error = "OSL texture evaluation currently requires Vulkan";
            return false;
        }
        const auto stage = ShaderCompiler::LoadComputeSpirv(validatedSpirv, error);
        if (!stage)
            return false;
        const auto& resources = stage->Description;
        if (!resources || resources->Buffers.size() != 2 || !resources->Uniforms.empty() || !resources->Textures.empty() ||
            !resources->Samplers.empty() || !resources->LoadStoreTextures.empty() || !resources->AccelerationStructures.empty())
        {
            error = "OSL sample ABI v1 expects two storage buffers";
            return false;
        }
        uint32_t bindings = 0;
        for (const auto& [name, buffer] : resources->Buffers)
        {
            if (buffer.Set != 0 || buffer.Slot > 1)
            {
                error = "OSL sample ABI v1 expects buffer bindings 0 and 1 in set 0";
                return false;
            }
            bindings |= 1u << buffer.Slot;
        }
        if (bindings != 3)
        {
            error = "OSL sample ABI v1 has duplicate buffer bindings";
            return false;
        }
        try
        {
            Vector<uint32_t> words(validatedSpirv.size() / sizeof(uint32_t));
            std::memcpy(words.data(), validatedSpirv.data(), validatedSpirv.size());
            const spirv_cross::Compiler reflection(words);
            if (reflection.get_execution_mode_argument(spv::ExecutionModeLocalSize, 0) != 64 ||
                reflection.get_execution_mode_argument(spv::ExecutionModeLocalSize, 1) != 1 ||
                reflection.get_execution_mode_argument(spv::ExecutionModeLocalSize, 2) != 1)
            {
                error = "OSL sample ABI v1 expects a 64x1x1 workgroup";
                return false;
            }
            for (const auto& buffer : reflection.get_shader_resources().storage_buffers)
            {
                const auto& block = reflection.get_type(buffer.base_type_id);
                const uint32_t slot = reflection.get_decoration(buffer.id, spv::DecorationBinding);
                if (block.member_types.size() != 1 || reflection.type_struct_member_offset(block, 0) != 0 ||
                    reflection.type_struct_member_array_stride(block, 0) != (slot == 0 ? 32u : 16u))
                {
                    error = "OSL sample ABI v1 buffer stride/layout mismatch";
                    return false;
                }
            }
        }
        catch (const std::exception& exception)
        {
            error = String("OSL sample ABI reflection failed: ") + exception.what();
            return false;
        }
        ShaderRenderPassDesc description{};
        description.ComputeShader = stage;
        const auto pass = ShaderRenderPass::Create(description);
        pass->Compile();
        m_Pipeline = pass->GetComputePipeline();
        if (m_Pipeline)
            m_Uniforms = UniformParams::Create(m_Pipeline);
        if (!m_Uniforms)
        {
            m_Pipeline = nullptr;
            error = "Could not create OSL compute pipeline";
            return false;
        }
        return true;
    }

    bool OslTextureProgram::Dispatch(const Ref<GenericGpuBuffer>& samples, const Ref<GenericGpuBuffer>& colors, String& error,
                                     const Ref<CommandBuffer>& commandBuffer)
    {
        error.clear();
        if (!m_Pipeline || !m_Uniforms || !samples || !colors || samples == colors || samples->GetBufferSize() == 0 ||
            samples->GetBufferSize() % sizeof(OslTextureSample) != 0 ||
            colors->GetBufferSize() / sizeof(glm::vec4) != samples->GetBufferSize() / sizeof(OslTextureSample) ||
            colors->GetBufferSize() % sizeof(glm::vec4) != 0)
        {
            error = "OSL dispatch requires an initialized program and matching sample/color buffers";
            return false;
        }
        const uint32_t count = samples->GetBufferSize() / sizeof(OslTextureSample);
        if (count > 65535u * 64u)
        {
            error = "OSL sample batch exceeds the portable dispatch limit";
            return false;
        }
        m_Uniforms->SetBuffer(0, 0, samples);
        m_Uniforms->SetBuffer(0, 1, colors);
        auto& api = RenderAPI::Get();
        api.SetComputePipeline(m_Pipeline, commandBuffer);
        api.SetUniforms(m_Uniforms, commandBuffer);
        api.DispatchCompute((count + 63u) / 64u, 1, 1, commandBuffer);
        return true;
    }
} // namespace Crowny

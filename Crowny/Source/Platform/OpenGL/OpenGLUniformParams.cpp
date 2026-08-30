#include "cwpch.h"

#include "Platform/OpenGL/OpenGLUniformParams.h"

#include "Platform/OpenGL/OpenGLGpuBuffer.h"
#include "Platform/OpenGL/OpenGLSamplerState.h"
#include "Platform/OpenGL/OpenGLTexture.h"
#include "Platform/OpenGL/OpenGLUniformBufferBlock.h"
#include "Platform/OpenGL/OpenGLUtils.h"

#include <glad/glad.h>

namespace Crowny
{
    namespace
    {
        void ClearTextureUnit(uint32_t unit)
        {
            if (glad_glBindTextureUnit != nullptr)
            {
                glBindTextureUnit(unit, 0);
                return;
            }

            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_1D, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glBindTexture(GL_TEXTURE_3D, 0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
            glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
            glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
        }
    } // namespace

    OpenGLTextureBindingPlan BuildOpenGLTextureBindingPlan(uint32_t firstUnit, uint32_t maximumTextureUnits, uint32_t reflectedArraySize,
                                                           bool runtimeArray, uint32_t configuredArraySize, bool singleTextureAssigned)
    {
        OpenGLTextureBindingPlan plan;
        plan.FirstUnit = firstUnit;
        if (firstUnit >= maximumTextureUnits)
            return plan;

        const uint32_t assignedCount = configuredArraySize != 0 ? configuredArraySize : singleTextureAssigned ? 1u : 0u;
        const uint32_t requestedCount = runtimeArray ? std::max(assignedCount, 1u) : std::max(reflectedArraySize, 1u);
        plan.UnitCount = std::min(requestedCount, maximumTextureUnits - firstUnit);
        plan.AssignedCount = std::min(assignedCount, plan.UnitCount);
        return plan;
    }

    OpenGLUniformParams::OpenGLUniformParams(const Ref<UniformParamInfo>& desc) : UniformParams(desc)
    {
        GLint maximumTextureUnits = 0;
        glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maximumTextureUnits);
        m_MaximumTextureUnits = static_cast<uint32_t>(std::max(maximumTextureUnits, 0));
    }

    void OpenGLUniformParams::Bind() const
    {
        using ParamType = UniformParamInfo::ParamType;
        for (uint32_t index = 0; index < m_ParamInfo->GetNumElements(ParamType::ParamBlock); ++index)
        {
            const Ref<UniformBufferBlock>& block = m_BufferBlocks[index];
            uint32_t set = 0, slot = 0;
            m_ParamInfo->GetBinding(ParamType::ParamBlock, index, set, slot);
            uint32_t rendererId = 0;
            if (block)
            {
                block->FlushToGpu();
                rendererId = static_cast<const OpenGLUniformBufferBlock*>(block.get())->GetRendererID();
            }
            glBindBufferBase(GL_UNIFORM_BUFFER, OpenGLUtils::FlattenBinding(set, slot), rendererId);
        }

        for (uint32_t index = 0; index < m_ParamInfo->GetNumElements(ParamType::Texture); ++index)
        {
            uint32_t set = 0, slot = 0;
            m_ParamInfo->GetBinding(ParamType::Texture, index, set, slot);
            const uint32_t binding = OpenGLUtils::FlattenBinding(set, slot);
            const Vector<TextureData>& array = m_SampledTextureArrays[index];
            const TextureData& single = m_SampledTextureData[index];
            const OpenGLTextureBindingPlan plan =
              BuildOpenGLTextureBindingPlan(binding, m_MaximumTextureUnits, m_ParamInfo->GetArraySize(set, slot),
                                            m_ParamInfo->IsRuntimeArray(set, slot), static_cast<uint32_t>(array.size()), single.Texture != nullptr);
            for (uint32_t element = 0; element < plan.UnitCount; element++)
            {
                const TextureData* data = nullptr;
                if (!array.empty() && element < plan.AssignedCount)
                    data = &array[element];
                else if (array.empty() && element == 0 && plan.AssignedCount != 0)
                    data = &single;

                if (data != nullptr && data->Texture)
                    static_cast<const OpenGLTexture*>(data->Texture.get())->Bind(plan.FirstUnit + element);
                else
                    ClearTextureUnit(plan.FirstUnit + element);
            }
        }
        glActiveTexture(GL_TEXTURE0);

        for (uint32_t index = 0; index < m_ParamInfo->GetNumElements(ParamType::SamplerState); ++index)
        {
            uint32_t set = 0, slot = 0;
            m_ParamInfo->GetBinding(ParamType::SamplerState, index, set, slot);
            const Ref<SamplerState>& sampler = m_SamplerStates[index];
            const uint32_t binding = OpenGLUtils::FlattenBinding(set, slot);
            const uint32_t samplerCount = m_ParamInfo->IsRuntimeArray(set, slot) ? 1u : std::max(m_ParamInfo->GetArraySize(set, slot), 1u);
            const uint32_t boundSamplerCount = binding < m_MaximumTextureUnits ? std::min(samplerCount, m_MaximumTextureUnits - binding) : 0u;
            const uint32_t rendererId = sampler ? static_cast<const OpenGLSamplerState*>(sampler.get())->GetRendererID() : 0;
            for (uint32_t element = 0; element < boundSamplerCount; element++)
                glBindSampler(binding + element, rendererId);
        }

        if (GLAD_GL_VERSION_4_2)
        {
            for (uint32_t index = 0; index < m_ParamInfo->GetNumElements(ParamType::LoadStoreTexture); ++index)
            {
                const TextureData& data = m_LoadStoreTextures[index];
                uint32_t set = 0, slot = 0;
                m_ParamInfo->GetBinding(ParamType::LoadStoreTexture, index, set, slot);
                const OpenGLTexture* texture = static_cast<const OpenGLTexture*>(data.Texture.get());
                if (texture != nullptr)
                {
                    const uint32_t faceCount = data.Surface.NumFaces != 0 ? data.Surface.NumFaces : std::max(texture->GetDesc().Faces, 1u);
                    glBindImageTexture(OpenGLUtils::FlattenBinding(set, slot), texture->GetRendererID(), data.Surface.MipLevel,
                                       faceCount > 1 ? GL_TRUE : GL_FALSE, data.Surface.Face, GL_READ_WRITE,
                                       texture->GetOpenGLFormat().InternalFormat);
                }
                else
                    glBindImageTexture(OpenGLUtils::FlattenBinding(set, slot), 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
            }
        }

        if (GLAD_GL_VERSION_4_3)
        {
            for (uint32_t index = 0; index < m_ParamInfo->GetNumElements(ParamType::Buffer); ++index)
            {
                const Ref<GenericGpuBuffer>& buffer = m_Buffers[index];
                uint32_t set = 0, slot = 0;
                m_ParamInfo->GetBinding(ParamType::Buffer, index, set, slot);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, OpenGLUtils::FlattenBinding(set, slot),
                                 buffer ? static_cast<const OpenGLGenericGpuBuffer*>(buffer.get())->GetRendererID() : 0);
            }
        }
    }
} // namespace Crowny

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
    void OpenGLUniformParams::Bind() const
    {
        using ParamType = UniformParamInfo::ParamType;
        for (uint32_t index = 0; index < m_ParamInfo->GetNumElements(ParamType::ParamBlock); ++index)
        {
            const Ref<UniformBufferBlock>& block = m_BufferBlocks[index];
            if (!block)
                continue;
            uint32_t set = 0, slot = 0;
            m_ParamInfo->GetBinding(ParamType::ParamBlock, index, set, slot);
            block->FlushToGpu();
            const OpenGLUniformBufferBlock* glBlock = static_cast<const OpenGLUniformBufferBlock*>(block.get());
            glBindBufferBase(GL_UNIFORM_BUFFER, OpenGLUtils::FlattenBinding(set, slot), glBlock->GetRendererID());
        }

        for (uint32_t index = 0; index < m_ParamInfo->GetNumElements(ParamType::Texture); ++index)
        {
            uint32_t set = 0, slot = 0;
            m_ParamInfo->GetBinding(ParamType::Texture, index, set, slot);
            const uint32_t binding = OpenGLUtils::FlattenBinding(set, slot);
            const Vector<TextureData>& array = m_SampledTextureArrays[index];
            if (!array.empty())
            {
                GLint maximumTextureUnits = 0;
                glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maximumTextureUnits);
                const uint32_t count = std::min<uint32_t>(static_cast<uint32_t>(array.size()),
                                                          std::max(maximumTextureUnits - static_cast<GLint>(binding), 0));
                for (uint32_t element = 0; element < count; element++)
                {
                    if (array[element].Texture)
                        static_cast<const OpenGLTexture*>(array[element].Texture.get())->Bind(binding + element);
                }
                continue;
            }
            const TextureData& data = m_SampledTextureData[index];
            if (data.Texture)
                static_cast<const OpenGLTexture*>(data.Texture.get())->Bind(binding);
            else
            {
                glActiveTexture(GL_TEXTURE0 + binding);
                glBindTexture(GL_TEXTURE_2D, 0);
            }
        }

        for (uint32_t index = 0; index < m_ParamInfo->GetNumElements(ParamType::SamplerState); ++index)
        {
            uint32_t set = 0, slot = 0;
            m_ParamInfo->GetBinding(ParamType::SamplerState, index, set, slot);
            const Ref<SamplerState>& sampler = m_SamplerStates[index];
            glBindSampler(OpenGLUtils::FlattenBinding(set, slot),
                          sampler ? static_cast<const OpenGLSamplerState*>(sampler.get())->GetRendererID() : 0);
        }

        if (GLAD_GL_VERSION_4_2)
        {
            for (uint32_t index = 0; index < m_ParamInfo->GetNumElements(ParamType::LoadStoreTexture); ++index)
            {
                const TextureData& data = m_LoadStoreTextures[index];
                if (!data.Texture)
                    continue;
                uint32_t set = 0, slot = 0;
                m_ParamInfo->GetBinding(ParamType::LoadStoreTexture, index, set, slot);
                const OpenGLTexture* texture = static_cast<const OpenGLTexture*>(data.Texture.get());
                glBindImageTexture(OpenGLUtils::FlattenBinding(set, slot), texture->GetRendererID(), data.Surface.MipLevel,
                                   data.Surface.NumFaces > 1 ? GL_TRUE : GL_FALSE, data.Surface.Face, GL_READ_WRITE,
                                   texture->GetOpenGLFormat().InternalFormat);
            }
        }

        if (GLAD_GL_VERSION_4_3)
        {
            for (uint32_t index = 0; index < m_ParamInfo->GetNumElements(ParamType::Buffer); ++index)
            {
                const Ref<GenericGpuBuffer>& buffer = m_Buffers[index];
                if (!buffer)
                    continue;
                uint32_t set = 0, slot = 0;
                m_ParamInfo->GetBinding(ParamType::Buffer, index, set, slot);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, OpenGLUtils::FlattenBinding(set, slot),
                                 static_cast<const OpenGLGenericGpuBuffer*>(buffer.get())->GetRendererID());
            }
        }
    }
} // namespace Crowny

#include "cwpch.h"

#include "Crowny/RenderAPI/UniformParams.h"
#include "Crowny/Renderer/Material.h"

namespace Crowny
{

    Material::Material(const AssetHandle<Shader>& shader) : m_Shader(shader)
    {
        // TODO: Only cache the shader and remove this
        // TODO: Figure out when to compile.
        m_Shader->GetTechniques()[0]->GetRenderPasses()[0]->Compile();
        m_GraphicsPipeline = m_Shader->GetTechniques()[0]->GetRenderPasses()[0]->GetGraphicsPipeline();
        CreateAndAppendUniforms();
    }

    Ref<Material> Material::Create(const AssetHandle<Shader>& shader) { return CreateRef<Material>(shader); }

    void Material::CreateAndAppendUniforms()
    {
        const Ref<UniformParamInfo>& uniformParamInfo = m_GraphicsPipeline->GetParamInfo();
        m_Uniforms = UniformParams::Create(m_GraphicsPipeline);
        for (uint32_t i = 0; i < SHADER_COUNT; i++)
        {
            const Ref<UniformDesc>& paramDesc = uniformParamInfo->GetUniformDesc((ShaderType)i);
            if (!paramDesc)
                continue;
            for (const auto& [name, uniformBuffer] : paramDesc->Uniforms)
            {
                // m_Bindings[name] = UniformBinding{ uniformBuffer.Set, uniformBuffer.Slot };
                for (uint32_t i = 0; i < uniformBuffer.Members.size(); i++)
                    m_Bindings[uniformBuffer.Members[i].Name] =
                      UniformMember{ uniformBuffer.Members[i].Offset, uniformBuffer.Members[i].DataType, name };

                m_UniformBlocks[name] =
                  UniformBufferBlock::Create(uniformBuffer.BlockSize, BufferUsage::DYNAMIC_DRAW); // TODO: To dynamic or not dynamic?
                m_Uniforms->SetUniformBlockBuffer(name, m_UniformBlocks[name]);                   // TODO: Avoid the double lookup
            }
        }
    }

    void Material::Bind()
    {
        for (const auto& [_, block] : m_UniformBlocks)
            block->FlushToGpu();
    }

    void Material::SetFloat(const String& name, float value)
    {
        const auto iterFind = m_Bindings.find(name);
        if (iterFind == m_Bindings.cend())
        {
            CW_ENGINE_WARN("Could not find uniform {}", name);
            return;
        }
        if (iterFind->second.DataType != ShaderDataType::Float)
        {
            CW_ENGINE_WARN("Trying to write the wrong data type {}, expected {}, got float", name, ShaderDataTypeToString(iterFind->second.DataType));
            return;
        }
        m_UniformBlocks[iterFind->second.BufferName]->Write(iterFind->second.Offset, &value, sizeof(value));
    }

    void Material::SetColor(const String& name, const glm::vec4& value)
    {
        const auto iterFind = m_Bindings.find(name);
        if (iterFind == m_Bindings.cend())
        {
            CW_ENGINE_WARN("Could not find uniform {}", name);
            return;
        }
        if (iterFind->second.DataType != ShaderDataType::Float3)
        {
            CW_ENGINE_WARN("Trying to write the wrong data type {}, expected {}, got color", name, ShaderDataTypeToString(iterFind->second.DataType));
            return;
        }
        m_UniformBlocks[iterFind->second.BufferName]->Write(iterFind->second.Offset, &value, sizeof(value));
    }

    void Material::SetMatrix(const String& name, const glm::mat4& value)
    {
        const auto iterFind = m_Bindings.find(name);
        if (iterFind == m_Bindings.cend())
        {
            CW_ENGINE_WARN("Could not find uniform {}", name);
            return;
        }
        if (iterFind->second.DataType != ShaderDataType::Mat4)
        {
            CW_ENGINE_WARN("Trying to write the wrong data type {}, expected {}, got matrix", name,
                           ShaderDataTypeToString(iterFind->second.DataType));
            return;
        }
        m_UniformBlocks[iterFind->second.BufferName]->Write(iterFind->second.Offset, &value, sizeof(value));
    }

    void Material::SetTexture(const String& name, const AssetHandle<Texture>& texture)
    {
        // TODO: Do shaders properly, don't just write into frag.
        m_Uniforms->SetTexture(FRAGMENT_SHADER, name, texture.GetInternalPtr());
    }

    void Material::SetTexture(const String& name, const Ref<Texture>& texture)
    {
        // TODO: Do shaders properly, don't just write into frag.
        m_Uniforms->SetTexture(FRAGMENT_SHADER, name, texture);
    }

} // namespace Crowny
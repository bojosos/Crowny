#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Assets/AssetListener.h"

#include "Crowny/RenderAPI/GpuBuffer.h"
#include "Crowny/RenderAPI/SamplerState.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/UniformBufferBlock.h"
#include "Crowny/RenderAPI/UniformParams.h"

#include "Crowny/Common/StdHeaders.h"
#include "Crowny/Common/Types.h"

namespace Crowny
{
    template <typename T> 
    struct MaterialDataParam
    {
        MaterialDataParam() : Size(sizeof(T)) { Data = malloc(sizeof(T)); }

        uint32_t Size = 0;
        void* Data = nullptr;
    };

    class Material : public Asset, public AssetListener
    {
    public:
        // A map from uniform buffer member name to uniform buffer name.
        struct UniformMember
        {
            uint32_t Offset;
            ShaderDataType DataType;
            String BufferName;
        };

        Material(const AssetHandle<Shader>& shader);
        virtual ~Material() override = default;

        static Ref<Material> Create(const AssetHandle<Shader>& shader);

        AssetHandle<Shader> GetShader() { return m_Shader; }
        virtual void GetAssets(Vector<AssetHandle<Asset>>& assets) override { assets.push_back(m_Shader); }

        void SetShader(const AssetHandle<Shader>& shader);
        void ReloadParams();

        UnorderedMap<String, UniformMember> GetBindings() const { return m_Bindings; }

        template <typename T>
        T GetDataParam(const String& name)
        {
            const auto iterFind = m_Bindings.find(name);
            if (iterFind == m_Bindings.cend())
            {
                CW_ENGINE_WARN("Could not find uniform {}", name);
                return T();
            }
            if (iterFind->second.DataType != ShaderDataType::Float)
            {
                CW_ENGINE_WARN("Trying to write the wrong data type {}, expected {}, got float", name,
                               ShaderDataTypeToString(iterFind->second.DataType));
                return T();
            }
            T value;
            m_UniformBlocks[iterFind->second.BufferName]->Read(iterFind->second.Offset, &value, sizeof(value));
            return value;
        }

        Ref<Texture> GetTexture(uint32_t set, uint32_t slot) const { return m_Uniforms->GetTexture(set, slot); }
        UnorderedMap<String, UniformResourceDesc> GetTextures() const { return m_GraphicsPipeline->GetParamInfo()->GetUniformDesc(FRAGMENT_SHADER)->Textures; }

        void FlushUniformBuffers();
        void SetFloat(const String& name, float value);
        void SetFloat2(const String& name, const glm::vec2& value);
        void SetInt(const String& name, int value);
        void SetColor(const String& name, const glm::vec4& color);
        void SetVector3(const String& name, const glm::vec3& value);
        void SetMatrix(const String& name, const glm::mat4& matrix);
        void SetTexture(const String& name, const AssetHandle<Texture>& texture);
        void SetTexture(const String& name, const Ref<Texture>& texture);
        const Ref<UniformParams>& GetUniformParams() const { return m_Uniforms; }
        const Ref<GraphicsPipeline>& GetGraphicsPipeline() const { return m_GraphicsPipeline; }

    private:
        void CreateAndAppendUniforms();

    private:
        Ref<GraphicsPipeline> m_GraphicsPipeline;
        // TODO: render passes, each of these 3 should be per render pass
        UnorderedMap<String, Ref<UniformBufferBlock>> m_UniformBlocks;
        Ref<UniformParams> m_Uniforms;

        UnorderedMap<String, UniformMember> m_Bindings;

        AssetHandle<Shader> m_Shader;
    };
} // namespace Crowny

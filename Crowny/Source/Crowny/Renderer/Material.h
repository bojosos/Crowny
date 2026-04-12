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
    template <typename T> struct MaterialDataParam
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

        struct PassData
        {
            Ref<GraphicsPipeline> Pipeline;
            Ref<UniformParams> Uniforms;
            UnorderedMap<String, Ref<UniformBufferBlock>> UniformBlocks;
        };

        Material(const AssetHandle<Shader>& shader);
        virtual ~Material() override = default;

        virtual AssetType GetAssetType() const override { return AssetType::Material; }
        static AssetType GetStaticType() { return AssetType::Material; }

        static Ref<Material> Create(const AssetHandle<Shader>& shader);
        static Ref<Material> CreatePBR(const AssetHandle<Shader>& shader);
        static Ref<Material> CreateToon(const AssetHandle<Shader>& shader);
        static Ref<Material> CreateUnlit(const AssetHandle<Shader>& shader);

        AssetHandle<Shader> GetShader() const { return m_Shader; }
        virtual void GetAssets(Vector<AssetHandle<Asset>>& assets) override { assets.push_back(m_Shader); }

        void SetShader(const AssetHandle<Shader>& shader);
        void ReloadParams();

        const UnorderedMap<String, UniformMember>& GetBindings() const { return m_Bindings; }
        bool HasBinding(const String& name) const { return m_Bindings.find(name) != m_Bindings.cend(); }

        template <typename T> T GetDataParam(const String& name) const
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
            // Read from first pass that has it
            for (const auto& pass : m_Passes)
            {
                const auto blockIt = pass.UniformBlocks.find(iterFind->second.BufferName);
                if (blockIt != pass.UniformBlocks.end())
                {
                    blockIt->second->Read(iterFind->second.Offset, &value, sizeof(value));
                    return value;
                }
            }
            return T();
        }

        Ref<Texture> GetTexture(uint32_t set, uint32_t slot) const { return m_Passes[0].Uniforms->GetTexture(set, slot); }
        UnorderedMap<String, UniformResourceDesc> GetTextures() const
        {
            return m_Passes[0].Pipeline->GetParamInfo()->GetUniformDesc(FRAGMENT_SHADER)->Textures;
        }

        void FlushUniformBuffers();
        void SetFloat(const String& name, float value);
        void SetFloat2(const String& name, const glm::vec2& value);
        void SetInt(const String& name, int value);
        void SetColor(const String& name, const glm::vec4& color);
        void SetVector3(const String& name, const glm::vec3& value);
        void SetMatrix(const String& name, const glm::mat4& matrix);
        void SetTexture(const String& name, const AssetHandle<Texture>& texture);
        void SetTexture(const String& name, const Ref<Texture>& texture);

        // Multi-pass accessors
        uint32_t GetPassCount() const { return (uint32_t)m_Passes.size(); }
        const Ref<UniformParams>& GetUniformParams(uint32_t pass = 0) const { return m_Passes[pass].Uniforms; }
        const Ref<GraphicsPipeline>& GetGraphicsPipeline(uint32_t pass = 0) const { return m_Passes[pass].Pipeline; }

    private:
        friend class cereal::access;
        Material() = default; // For serialization only
        void CreateAndAppendUniforms(uint32_t passIndex);

    private:
        CW_SERIALIZABLE(Material);

        Vector<PassData> m_Passes;
        UnorderedMap<String, UniformMember> m_Bindings;
        AssetHandle<Shader> m_Shader;
    };
} // namespace Crowny

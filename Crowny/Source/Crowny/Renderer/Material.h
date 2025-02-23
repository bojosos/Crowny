#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Assets/AssetListener.h"

#include "Crowny/RenderAPI/GpuBuffer.h"
#include "Crowny/RenderAPI/SamplerState.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/UniformBufferBlock.h"

#include "Crowny/Common/Types.h"
#include "Crowny/Common/StdHeaders.h"

namespace Crowny
{

    class Material : public Asset, public AssetListener
    {
    public:
        Material(const AssetHandle<Shader>& shader);
        virtual ~Material() override = default;

        static Ref<Material> Create(const AssetHandle<Shader> &shader);

        AssetHandle<Shader> GetShader() { return m_Shader; }
        virtual void GetAssets(Vector<AssetHandle<Asset>>& assets) override { assets.push_back(m_Shader); }


        void Bind();
        // void SetShader(const AssetHandle<Shader>& shader);
        void SetFloat(const String& name, float value);
        void SetColor(const String& name, const glm::vec4& color);
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
        // A map from uniform buffer member name to uniform buffer name.
        struct UniformMember
        {
            uint32_t Offset;
            ShaderDataType DataType;
            String BufferName;
        };
        UnorderedMap<String, UniformMember> m_Bindings;

        AssetHandle<Shader> m_Shader;
    };
} // namespace Crowny

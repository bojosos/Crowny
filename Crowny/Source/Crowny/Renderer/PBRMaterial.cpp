#include "cwpch.h"

#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/UniformBufferBlock.h"
#include "Crowny/RenderAPI/UniformParams.h"
#include "Crowny/Renderer/PBRMaterial.h"

namespace Crowny
{
    PBRMaterial::PBRMaterial(const AssetHandle<Shader>& shader) : Material(shader)
    {
        BufferLayout layout = { { ShaderDataType::Float3, "a_Position" },
                                { ShaderDataType::Float3, "a_Normal" },
                                { ShaderDataType::Float2, "a_Uv" },
                                { ShaderDataType::Float3, "a_Tangent" } };

        PipelineStateDesc desc;
        Ref<ShaderStage> vertex = shader->GetStage(VERTEX_SHADER);
        Ref<ShaderStage> fragment = shader->GetStage(FRAGMENT_SHADER);
        desc.VertexShader = vertex;
        desc.FragmentShader = fragment;

        m_Pipeline = GraphicsPipeline::Create(desc, layout);
        m_Uniforms = UniformParams::Create(m_Pipeline);

        m_MaterialParams = UniformBufferBlock::Create(fragment->GetUniformDesc()->Uniforms.at("Parameters").BlockSize,
                                                      BufferUsage::DYNAMIC_DRAW);
        m_Uniforms->SetUniformBlockBuffer(0, 11, m_MaterialParams);
        m_Textures.resize(5);
        SetAlbedo(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        SetMetalness(0.8f);
        SetRoughness(0.2f);
    }

    PBRMaterial::~PBRMaterial()
    {
        m_Uniforms = nullptr;
        m_Textures.clear();
    }

    void PBRMaterial::Bind()
    {
        auto& rapi = RenderAPI::Get();
        rapi.SetGraphicsPipeline(m_Pipeline);

        for (uint32_t i = 0; i < 5; i++)
        {
            m_Uniforms->SetTexture(0, 6 + i, m_Textures[i]);
        }
        if (m_HasChanged)
            m_MaterialParams->Write(0, &m_Params, sizeof(Params));
        rapi.SetUniforms(m_Uniforms);
    }

    void PBRMaterial::SetAlbedo(const glm::vec4& color)
    {
        m_Params.Albedo = color;
        m_HasChanged = true;
    }

    void PBRMaterial::SetMetalness(float value)
    {
        m_Params.Metalness = value;
        m_HasChanged = true;
    }

    void PBRMaterial::SetRoughness(float value)
    {
        m_Params.Roughness = value;
        m_HasChanged = true;
    }

    void PBRMaterial::SetAlbedoMap(const Ref<Texture>& texture)
    {
        m_Textures[0] = texture;
        // SetTexture("u_AlbedoMap", texture);
    }

    void PBRMaterial::SetMetalnessMap(const Ref<Texture>& texture)
    {
        m_Textures[1] = texture;
        // SetTexture("u_MetalnessMap", texture);
    }

    void PBRMaterial::SetNormalMap(const Ref<Texture>& texture)
    {
        m_Textures[3] = texture;
        // SetTexture("u_NormalMap", texture);
    }

    void PBRMaterial::SetRoughnessMap(const Ref<Texture>& texture)
    {
        m_Textures[2] = texture;
        // SetTexture("u_RoughnessMap", texture);
    }

    void PBRMaterial::SetAoMap(const Ref<Texture>& texture)
    {
        m_Textures[4] = texture;
        // SetTexture("u_AoMap", texture);
    }

    Ref<Texture> PBRMaterial::GetAlbedoMap() { return m_Textures[0]; }

    Ref<Texture> PBRMaterial::GetMetalnessMap() { return m_Textures[1]; }

    Ref<Texture> PBRMaterial::GetNormalMap() { return m_Textures[3]; }

    Ref<Texture> PBRMaterial::GetRoughnessMap() { return m_Textures[2]; }

    Ref<Texture> PBRMaterial::GetAoMap() { return m_Textures[4]; }
} // namespace Crowny
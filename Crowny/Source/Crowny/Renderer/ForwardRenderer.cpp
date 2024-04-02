#include "cwpch.h"

// #include "../../Crowny-Editor/Source/Panels/InspectorPanel.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/RenderAPI/GraphicsPipeline.h"
#include "Crowny/RenderAPI/RenderCommand.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/UniformBufferBlock.h"
#include "Crowny/RenderAPI/UniformParams.h"
#include "Crowny/RenderAPI/VertexArray.h"
#include "Crowny/RenderAPI/VertexBuffer.h"
#include "Crowny/Renderer/Camera.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/ForwardRenderer.h"
#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Renderer/Skybox.h"
#include "Crowny/Utils/ShaderCompiler.h"

#include <glm/gtc/type_ptr.hpp>

namespace Crowny
{
    struct ForwardRendererData
    {
        Ref<GraphicsPipeline> Pipeline;
        Ref<UniformBufferBlock> Mvp;
        Ref<UniformBufferBlock> GlobalFragmentParams;

        Ref<Skybox> Skybox;
        Ref<VertexBuffer> SkyboxVbo;
        Ref<IndexBuffer> SkyboxIbo;
        Ref<GraphicsPipeline> SkyboxPipeline;
        Ref<UniformParams> SkyboxUniforms;
        Ref<UniformBufferBlock> SkyboxMvp;

        Vector<Ref<Texture>> MeshTextures;
        Ref<UniformBufferBlock> MaterialParams;
        Ref<GraphicsPipeline> MeshPipeline;
        Ref<UniformParams> MeshUniforms;
        Ref<UniformBufferBlock> MeshMvp;
    };

    static ForwardRendererData* s_Data;

    float verts[] = { -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,
                      -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f,
                      -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f,
                      -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,
                      1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,
                      -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f };

    uint32_t inds[] = {
        0,  1,  2,  0,  2,  3,  4,  5,  6,  4,  6,  7,  8,  9,  10, 8,  10, 11,
        12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
    };

    void ForwardRenderer::Init()
    {
        s_Data = new ForwardRendererData();
        // Ref<UniformParams>& uniforms = InspectorPanel::GetSelectedMaterial()->GetUniformParams();

        AssetHandle<Shader> shader1 = AssetManager::Get().Load<Shader>(PBRIBL_SHADER_PATH);
        // Ref<Shader> shader1 = Importer::Get().Import<Shader>("Resources/Shaders/Pbribl.glsl");
        // AssetManager::Get().Save(shader1, PBRIBL_SHADER_PATH);

        Ref<ShaderStage> vertex = shader1->GetStage(VERTEX_SHADER);
        Ref<ShaderStage> fragment = shader1->GetStage(FRAGMENT_SHADER);

        s_Data->Mvp =
          UniformBufferBlock::Create(vertex->GetUniformDesc()->Uniforms.at("MVP").BlockSize, BufferUsage::DYNAMIC_DRAW);

        BufferLayout layout = { { ShaderDataType::Float3, "a_Position" },
                                { ShaderDataType::Float3, "a_Normal" },
                                { ShaderDataType::Float3, "a_Tangent" },
                                { ShaderDataType::Float3, "a_Bitangent" },
                                { ShaderDataType::Float2, "a_Uv" } };

        PipelineStateDesc desc;
        desc.VertexShader = vertex;
        desc.FragmentShader = fragment;

        s_Data->MeshPipeline = GraphicsPipeline::Create(desc, layout);
        s_Data->MeshUniforms = UniformParams::Create(s_Data->MeshPipeline);

        s_Data->MaterialParams = UniformBufferBlock::Create(
          fragment->GetUniformDesc()->Uniforms.at("Parameters").BlockSize, BufferUsage::DYNAMIC_DRAW);
        s_Data->MeshUniforms->SetUniformBlockBuffer(0, 11, s_Data->MaterialParams);
        s_Data->MeshTextures.resize(5);

        s_Data->MeshUniforms->SetUniformBlockBuffer(ShaderType::VERTEX_SHADER, "MVP", s_Data->Mvp);

        s_Data->GlobalFragmentParams = UniformBufferBlock::Create(
          fragment->GetUniformDesc()->Uniforms.at("UBOParams").BlockSize, BufferUsage::DYNAMIC_DRAW);
        s_Data->MeshUniforms->SetUniformBlockBuffer(0, 2, s_Data->GlobalFragmentParams);

        s_Data->Skybox = CreateRef<Skybox>("Resources/Textures/envmap.hdr");
        s_Data->SkyboxVbo = VertexBuffer::Create(verts, sizeof(verts));
        s_Data->SkyboxVbo->SetLayout({ { ShaderDataType::Float3, "a_Pos" } });
        s_Data->SkyboxIbo = IndexBuffer::Create(inds, sizeof(inds) / sizeof(uint32_t));

        AssetHandle<Shader> shader = AssetManager::Get().Load<Shader>(SKYBOX_SHADER_PATH);
        // shader = Importer::Get().Import<Shader>(SKYBOX_SHADER_PATH);
        Ref<ShaderStage> skyboxVertex = shader->GetStage(VERTEX_SHADER);
        Ref<ShaderStage> skyboxFragment = shader->GetStage(FRAGMENT_SHADER);

        PipelineStateDesc skyboxDesc;
        skyboxDesc.VertexShader = skyboxVertex;
        skyboxDesc.FragmentShader = skyboxFragment;
        skyboxDesc.DepthStencilState.EnableDepthRead = false;
        skyboxDesc.DepthStencilState.EnableDepthWrite = false;
        s_Data->SkyboxPipeline = GraphicsPipeline::Create(skyboxDesc, s_Data->SkyboxVbo->GetLayout());
        s_Data->SkyboxUniforms = UniformParams::Create(s_Data->SkyboxPipeline);

        s_Data->SkyboxMvp = UniformBufferBlock::Create(skyboxVertex->GetUniformDesc()->Uniforms.at("MVP").BlockSize,
                                                       BufferUsage::DYNAMIC_DRAW);
        s_Data->SkyboxUniforms->SetUniformBlockBuffer(0, 0, s_Data->SkyboxMvp);
        s_Data->SkyboxUniforms->SetUniformBlockBuffer(0, 1, s_Data->GlobalFragmentParams);
        s_Data->SkyboxUniforms->SetTexture(0, 2, s_Data->Skybox->m_EnvironmentMap);
    }

    void ForwardRenderer::Begin() {}

    void ForwardRenderer::BeginScene(const Camera& camera, const glm::mat4& viewMatrix)
    {
        auto& rapi = RenderAPI::Get();
        rapi.SetGraphicsPipeline(s_Data->Pipeline);
        rapi.ClearViewport(FBT_COLOR | FBT_DEPTH);
        s_Data->Mvp->Write(0, glm::value_ptr(camera.GetProjection()), sizeof(glm::mat4));
        s_Data->Mvp->Write(sizeof(glm::mat4), glm::value_ptr(viewMatrix), sizeof(glm::mat4));

        // Ref<UniformParams> uniforms = InspectorPanel::GetSelectedMaterial()->GetUniformParams();
        s_Data->MeshUniforms->SetTexture(0, 3, s_Data->Skybox->m_IrradianceMap);
        s_Data->MeshUniforms->SetTexture(0, 4, s_Data->Skybox->m_Brdf);
        s_Data->MeshUniforms->SetTexture(0, 5, s_Data->Skybox->m_PrefilteredMap);

        glm::vec4 lightPositions[] = { glm::vec4(-10.0f, 10.0f, 10.0f, 1.0f), glm::vec4(10.0f, 10.0f, 10.0f, 1.0f),
                                       glm::vec4(-10.0f, -10.0f, 10.0f, 1.0f), glm::vec4(10.0f, -10.0f, 10.0f, 1.0f) };
        glm::vec3 lightColors[] = { glm::vec3(300.0f, 300.0f, 300.0f), glm::vec3(300.0f, 300.0f, 300.0f),
                                    glm::vec3(300.0f, 300.0f, 300.0f), glm::vec3(300.0f, 300.0f, 300.0f) };
        float gamma = 2.2f, exposure = 4.5f;
        s_Data->GlobalFragmentParams->Write(0, lightPositions, sizeof(lightPositions));
        s_Data->GlobalFragmentParams->Write(sizeof(lightPositions), &gamma, sizeof(float));
        s_Data->GlobalFragmentParams->Write(sizeof(lightPositions) + sizeof(float), &exposure, sizeof(float));
        // glm::vec3 camPos = glm::inverse(viewMatrix)[3];
        const glm::vec3 camPos = camera.GetPosition();
        s_Data->GlobalFragmentParams->Write(sizeof(lightPositions) + 2 * sizeof(float), &camPos, sizeof(glm::vec3));

        s_Data->SkyboxMvp->Write(0, glm::value_ptr(camera.GetProjection()), sizeof(glm::mat4));
        glm::mat4 inv = glm::mat4(glm::mat3(viewMatrix));
        s_Data->SkyboxMvp->Write(sizeof(glm::mat4), glm::value_ptr(inv), sizeof(glm::mat4));

        rapi.SetGraphicsPipeline(s_Data->SkyboxPipeline);
        rapi.SetVertexBuffers(0, &s_Data->SkyboxVbo, 1);
        rapi.SetIndexBuffer(s_Data->SkyboxIbo);

        rapi.SetUniforms(s_Data->SkyboxUniforms);
        rapi.DrawIndexed(0, s_Data->SkyboxIbo->GetCount(), 0, 1);
    }

    void ForwardRenderer::SubmitLightSetup() {}

    void ForwardRenderer::Submit(const AssetHandle<Mesh>& mesh, const glm::mat4& transform)
    {
        // const glm::mat4 transform(1.0f);
        RenderAPI& rapi = RenderAPI::Get();

        rapi.SetGraphicsPipeline(s_Data->MeshPipeline);

        TextureParameters pars;
        pars.Width = 1;
        pars.Height = 1;
        pars.Format = TextureFormat::RGBA8;
        Ref<Texture> text = Texture::Create(pars);
        Ref<PixelData> dat = text->AllocatePixelData(0, 0);
        dat->SetColorAt(0, 0, 0, glm::vec4(1.0f));
        text->WriteData(*dat);
        for (uint32_t i = 0; i < 5; i++)
            s_Data->MeshUniforms->SetTexture(0, 6 + i, text);
        // if (m_HasChanged)
        struct Params
        {
            glm::vec4 Albedo = glm::vec4(1.0f);
            float Roughness = 0.2f;
            float Metalness = 0.8f;
        };

        Params params;
        s_Data->MaterialParams->Write(0, &params, sizeof(Params));
        s_Data->Mvp->Write(sizeof(glm::mat4) * 2, glm::value_ptr(transform), sizeof(glm::mat4));
        rapi.SetUniforms(s_Data->MeshUniforms);

        rapi.SetVertexBuffers(0, &mesh->GetVertexBuffer(), 1);
        rapi.SetIndexBuffer(mesh->GetIndexBuffer());
        rapi.SetDrawMode(mesh->GetDrawMode());
        // rapi.DrawIndexed(0, mesh->GetIndexCount(), 0, mesh->GetVertexCount());
        rapi.Draw(0, mesh->GetVertexCount());
    }

    void ForwardRenderer::SubmitMesh(const Ref<Mesh>& mesh, const glm::mat4& transform)
    {
        // s_Data->Mvp->Write(sizeof(glm::mat4) * 2, glm::value_ptr(transform), sizeof(glm::mat4));
        // mesh->Draw();
    }

    void ForwardRenderer::EndScene() {}

    void ForwardRenderer::End() {}

    void ForwardRenderer::Flush() {}

    void ForwardRenderer::Shutdown()
    {
        // s_Data->Pipeline = nullptr;
        // s_Data->Mvp = nullptr;
        // s_Data->GlobalFragmentParams = nullptr;
        // s_Data->Skybox = nullptr;
        // s_Data->SkyboxVbo = nullptr;
        // s_Data->SkyboxIbo = nullptr;
        // s_Data->SkyboxPipeline = nullptr;
        // s_Data->SkyboxUniforms = nullptr;
        // s_Data->SkyboxMvp = nullptr;
        // delete s_Data;
    }

} // namespace Crowny

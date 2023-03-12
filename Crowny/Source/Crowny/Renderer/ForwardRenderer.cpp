#include "cwpch.h"

// #include "../../Crowny-Editor/Source/Panels/InspectorPanel.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/RenderAPI/RenderCommand.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/VertexArray.h"
#include "Crowny/RenderAPI/VertexBuffer.h"
#include "Crowny/Renderer/Camera.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/ForwardRenderer.h"
#include "Crowny/Renderer/Model.h"
#include "Crowny/Renderer/Skybox.h"
#include "Crowny/Utils/ShaderCompiler.h"

#include <glm/gtc/type_ptr.hpp>

namespace Crowny
{
    struct ForwardRendererData
    {
        // Ref<GraphicsPipeline> Pipeline;
        // Ref<UniformBufferBlock> Mvp;
        // Ref<UniformBufferBlock> GlobalFragmentParams;
        //
        // Ref<Skybox> Skybox;
        // Ref<VertexBuffer> SkyboxVbo;
        // Ref<IndexBuffer> SkyboxIbo;
        // Ref<GraphicsPipeline> SkyboxPipeline;
        // Ref<UniformParams> SkyboxUniforms;
        // Ref<UniformBufferBlock> SkyboxMvp;
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
        // s_Data = new ForwardRendererData();
        // Ref<UniformParams>& uniforms = InspectorPanel::GetSelectedMaterial()->GetUniformParams();
        //
        // AssetHandle<Shader> shader = AssetManager::Get().Load<Shader>(PBRIBL_SHADER_PATH);
        // // Ref<Shader> shader = Importer::Get().Import<Shader>(PBRIBL_SHADER_PATH);
        // Ref<ShaderStage> vertex = shader->GetStage(VERTEX_SHADER);
        // Ref<ShaderStage> fragment = shader->GetStage(FRAGMENT_SHADER);
        //
        // s_Data->Mvp =
        //   UniformBufferBlock::Create(vertex->GetUniformDesc()->Uniforms.at("MVP").BlockSize,
        //   BufferUsage::DYNAMIC_DRAW);
        //
        // uniforms->SetUniformBlockBuffer(ShaderType::VERTEX_SHADER, "MVP", s_Data->Mvp);
        //
        // s_Data->GlobalFragmentParams = UniformBufferBlock::Create(
        //   fragment->GetUniformDesc()->Uniforms.at("UBOParams").BlockSize, BufferUsage::DYNAMIC_DRAW);
        // uniforms->SetUniformBlockBuffer(0, 2, s_Data->GlobalFragmentParams);
        //
        // s_Data->Skybox = CreateRef<Skybox>("Resources/Textures/envmap.hdr");
        // s_Data->SkyboxVbo = VertexBuffer::Create(verts, sizeof(verts));
        // s_Data->SkyboxVbo->SetLayout({ { ShaderDataType::Float3, "a_Pos" } });
        // s_Data->SkyboxIbo = IndexBuffer::Create(inds, sizeof(inds) / sizeof(uint32_t));
        //
        // shader = AssetManager::Get().Load<Shader>(SKYBOX_SHADER_PATH);
        // // shader = Importer::Get().Import<Shader>(SKYBOX_SHADER_PATH);
        // Ref<ShaderStage> skyboxVertex = shader->GetStage(VERTEX_SHADER);
        // Ref<ShaderStage> skyboxFragment = shader->GetStage(FRAGMENT_SHADER);
        //
        // PipelineStateDesc desc;
        // desc.VertexShader = skyboxVertex;
        // desc.FragmentShader = skyboxFragment;
        // desc.DepthStencilState.EnableDepthRead = false;
        // desc.DepthStencilState.EnableDepthWrite = false;
        // s_Data->SkyboxPipeline = GraphicsPipeline::Create(desc, s_Data->SkyboxVbo->GetLayout());
        // s_Data->SkyboxUniforms = UniformParams::Create(s_Data->SkyboxPipeline);
        //
        // s_Data->SkyboxMvp = UniformBufferBlock::Create(skyboxVertex->GetUniformDesc()->Uniforms.at("MVP").BlockSize,
        //                                                BufferUsage::DYNAMIC_DRAW);
        // s_Data->SkyboxUniforms->SetUniformBlockBuffer(0, 0, s_Data->SkyboxMvp);
        // s_Data->SkyboxUniforms->SetUniformBlockBuffer(0, 1, s_Data->GlobalFragmentParams);
        // s_Data->SkyboxUniforms->SetTexture(0, 2, s_Data->Skybox->m_EnvironmentMap);
    }

    void ForwardRenderer::Begin() {}

    void ForwardRenderer::BeginScene(const Camera& camera, const glm::mat4& viewMatrix)
    {
        // auto& rapi = RenderAPI::Get();
        // rapi.SetGraphicsPipeline(s_Data->Pipeline);
        // rapi.ClearViewport(FBT_COLOR | FBT_DEPTH);
        // s_Data->Mvp->Write(0, glm::value_ptr(camera.GetProjection()), sizeof(glm::mat4));
        // s_Data->Mvp->Write(sizeof(glm::mat4), glm::value_ptr(viewMatrix), sizeof(glm::mat4));
        //
        // Ref<UniformParams> uniforms = InspectorPanel::GetSelectedMaterial()->GetUniformParams();
        // uniforms->SetTexture(0, 3, s_Data->Skybox->m_IrradianceMap);
        // uniforms->SetTexture(0, 4, s_Data->Skybox->m_Brdf);
        // uniforms->SetTexture(0, 5, s_Data->Skybox->m_PrefilteredMap);
        //
        // glm::vec4 lightPositions[] = { glm::vec4(-10.0f, 10.0f, 10.0f, 1.0f), glm::vec4(10.0f, 10.0f, 10.0f, 1.0f),
        //                                glm::vec4(-10.0f, -10.0f, 10.0f, 1.0f), glm::vec4(10.0f, -10.0f, 10.0f, 1.0f)
        //                                };
        // glm::vec3 lightColors[] = { glm::vec3(300.0f, 300.0f, 300.0f), glm::vec3(300.0f, 300.0f, 300.0f),
        //                             glm::vec3(300.0f, 300.0f, 300.0f), glm::vec3(300.0f, 300.0f, 300.0f) };
        // float gamma = 2.2f, exposure = 4.5f;
        // glm::vec3 camPos = glm::inverse(viewMatrix)[3];
        // s_Data->GlobalFragmentParams->Write(0, lightPositions, sizeof(lightPositions));
        // s_Data->GlobalFragmentParams->Write(sizeof(lightPositions), &gamma, sizeof(float));
        // s_Data->GlobalFragmentParams->Write(sizeof(lightPositions) + sizeof(float), &exposure, sizeof(float));
        // s_Data->GlobalFragmentParams->Write(sizeof(lightPositions) + 2 * sizeof(float), &camPos, sizeof(glm::vec3));
        //
        // s_Data->SkyboxMvp->Write(0, glm::value_ptr(camera.GetProjection()), sizeof(glm::mat4));
        // glm::mat4 inv = glm::mat4(glm::mat3(viewMatrix));
        // s_Data->SkyboxMvp->Write(sizeof(glm::mat4), glm::value_ptr(inv), sizeof(glm::mat4));
        //
        // rapi.SetGraphicsPipeline(s_Data->SkyboxPipeline);
        // rapi.SetVertexBuffers(0, &s_Data->SkyboxVbo, 1);
        // rapi.SetIndexBuffer(s_Data->SkyboxIbo);
        //
        // rapi.SetUniforms(s_Data->SkyboxUniforms);
        // rapi.DrawIndexed(0, s_Data->SkyboxIbo->GetCount(), 0, 1);
    }

    void ForwardRenderer::SubmitLightSetup() {}

    void ForwardRenderer::Submit(const Ref<Model>& model, const glm::mat4& transform)
    {
        // s_Data->Mvp->Write(sizeof(glm::mat4) * 2, glm::value_ptr(transform), sizeof(glm::mat4));
        // InspectorPanel::GetSelectedMaterial()->Bind();
        // for (auto& mesh : model->GetMeshes())
        //     model->Draw();
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

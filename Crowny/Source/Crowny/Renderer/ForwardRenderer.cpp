#include "cwpch.h"

#include "../../Crowny-Editor/Source/Panels/ImGuiMaterialPanel.h"
#include "Crowny/RenderAPI/RenderCommand.h"
#include "Crowny/Renderer/ForwardRenderer.h"
#include "Crowny/Renderer/Skybox.h"
#include "Crowny/Utils/ShaderCompiler.h"

#include <glm/gtc/type_ptr.hpp>

namespace Crowny
{
    struct ForwardRendererData
    {
        Ref<VertexBuffer> VertexBuffer;
        Ref<GraphicsPipeline> Pipeline;
        Ref<UniformBufferBlock> Mvp;
        Ref<UniformBufferBlock> GlobalFragmentParams;
        Ref<Skybox> Skybox;
    };

    static ForwardRendererData* s_Data;

    void ForwardRenderer::Init()
    {
        s_Data = new ForwardRendererData();
        Ref<UniformParams>& uniforms = ImGuiMaterialPanel::GetSlectedMaterial()->GetUniformParams();
        ShaderCompiler compiler;
        Ref<Shader> vertex = Shader::Create(compiler.Compile("/Shaders/pbribl.vert", VERTEX_SHADER));
        Ref<Shader> fragment = Shader::Create(compiler.Compile("/Shaders/pbribl.frag", FRAGMENT_SHADER));

        s_Data->Mvp =
          UniformBufferBlock::Create(vertex->GetUniformDesc()->Uniforms.at("MVP").BlockSize, BufferUsage::DYNAMIC_DRAW);

        uniforms->SetUniformBlockBuffer(ShaderType::VERTEX_SHADER, "MVP", s_Data->Mvp);

        s_Data->GlobalFragmentParams = UniformBufferBlock::Create(
          fragment->GetUniformDesc()->Uniforms.at("UBOParams").BlockSize, BufferUsage::DYNAMIC_DRAW);
        uniforms->SetUniformBlockBuffer(0, 2, s_Data->GlobalFragmentParams);

        s_Data->Skybox = CreateRef<Skybox>("/Textures/envmap.hdr");
    }

    void ForwardRenderer::Begin() {}

    void ForwardRenderer::BeginScene(const Camera& camera, const glm::mat4& viewMatrix)
    {
        auto& rapi = RenderAPI::Get();
        rapi.SetGraphicsPipeline(s_Data->Pipeline);
        s_Data->Mvp->Write(0, glm::value_ptr(camera.GetProjection()), sizeof(glm::mat4));
        s_Data->Mvp->Write(sizeof(glm::mat4), glm::value_ptr(viewMatrix), sizeof(glm::mat4));

        Ref<UniformParams> uniforms = ImGuiMaterialPanel::GetSlectedMaterial()->GetUniformParams();
        uniforms->SetTexture(0, 3, s_Data->Skybox->m_IrradianceMap);
        uniforms->SetTexture(0, 4, s_Data->Skybox->m_Brdf);
        uniforms->SetTexture(0, 5, s_Data->Skybox->m_PrefilteredMap);

        glm::vec4 lightPositions[] = {
            glm::vec4(-10.0f, 10.0f, 10.0f, 1.0f),
            glm::vec4(10.0f, 10.0f, 10.0f, 1.0f),
            glm::vec4(-10.0f, -10.0f, 10.0f, 1.0f),
            glm::vec4(10.0f, -10.0f, 10.0f, 1.0f),
        };
        glm::vec3 lightColors[] = { glm::vec3(300.0f, 300.0f, 300.0f), glm::vec3(300.0f, 300.0f, 300.0f),
                                    glm::vec3(300.0f, 300.0f, 300.0f), glm::vec3(300.0f, 300.0f, 300.0f) };
        float gamma = 2.2f, exposure = 4.5f;
        glm::vec3 camPos = glm::inverse(viewMatrix)[3];
        s_Data->GlobalFragmentParams->Write(0, lightPositions, sizeof(lightPositions));
        s_Data->GlobalFragmentParams->Write(sizeof(lightPositions), &gamma, sizeof(float));
        s_Data->GlobalFragmentParams->Write(sizeof(lightPositions) + sizeof(float), &exposure, sizeof(float));
        s_Data->GlobalFragmentParams->Write(sizeof(lightPositions) + 2 * sizeof(float), &camPos, sizeof(glm::vec3));
    }

    void ForwardRenderer::SubmitLightSetup() {}

    void ForwardRenderer::Submit(const Ref<Model>& model, const glm::mat4& transform)
    {
        s_Data->Mvp->Write(sizeof(glm::mat4) * 2, glm::value_ptr(transform), sizeof(glm::mat4));
        ImGuiMaterialPanel::GetSlectedMaterial()->Bind();
        for (auto& mesh : model->GetMeshes())
            model->Draw();
    }

    void ForwardRenderer::SubmitMesh(const Ref<Mesh>& mesh, const glm::mat4& transform)
    {
        s_Data->Mvp->Write(sizeof(glm::mat4) * 2, glm::value_ptr(transform), sizeof(glm::mat4));
        mesh->Draw();
    }

    void ForwardRenderer::EndScene() {}

    void ForwardRenderer::End() {}

    void ForwardRenderer::Flush() {}

    void ForwardRenderer::Shutdown() { delete s_Data; }

} // namespace Crowny

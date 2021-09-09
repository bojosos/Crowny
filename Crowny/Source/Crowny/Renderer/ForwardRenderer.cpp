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
        Ref<UniformBufferBlock> FragmentMaterialParams;
        Ref<UniformBufferBlock> GlobalFragmentParams;
        Ref<UniformParams> Uniforms;
    };

    static ForwardRendererData* s_Data;

    void ForwardRenderer::Init()
    {
        s_Data = new ForwardRendererData();
        ShaderCompiler compiler;
        Ref<Shader> vertex = Shader::Create(compiler.Compile("/Shaders/pbribl.vert", VERTEX_SHADER));
        Ref<Shader> fragment = Shader::Create(compiler.Compile("/Shaders/pbribl.frag", FRAGMENT_SHADER));

        BufferLayout layout = { { ShaderDataType::Float3, "a_Position" },
                                { ShaderDataType::Float3, "a_Normal" },
                                { ShaderDataType::Float2, "a_Uv" },
                                { ShaderDataType::Float3, "a_Tangent" } };

        PipelineStateDesc desc;
        desc.FragmentShader = fragment;
        desc.VertexShader = vertex;

        s_Data->Pipeline = GraphicsPipeline::Create(desc, layout);
        s_Data->Mvp =
          UniformBufferBlock::Create(vertex->GetUniformDesc()->Uniforms.at("MVP").BlockSize, BufferUsage::DYNAMIC_DRAW);
        s_Data->Uniforms = UniformParams::Create(s_Data->Pipeline);
        s_Data->Uniforms->SetUniformBlockBuffer(ShaderType::VERTEX_SHADER, "MVP", s_Data->Mvp);

        s_Data->FragmentMaterialParams = UniformBufferBlock::Create(
          fragment->GetUniformDesc()->Uniforms.at("Parameters").BlockSize, BufferUsage::DYNAMIC_DRAW);
        s_Data->GlobalFragmentParams = UniformBufferBlock::Create(
          fragment->GetUniformDesc()->Uniforms.at("UBOParams").BlockSize, BufferUsage::DYNAMIC_DRAW);
        s_Data->Uniforms->SetUniformBlockBuffer(0, 11, s_Data->FragmentMaterialParams);
    }

    void ForwardRenderer::Begin() {}
    /*
        layout (binding = 2) uniform UBOParams {
            vec4 lights[4];
            float exposure;
            float gamma;
            vec3 camPos;
        } uboParams;

        layout (binding = 3) uniform samplerCube samplerIrradiance;
        layout (binding = 4) uniform sampler2D samplerBRDFLUT;
        layout (binding = 5) uniform samplerCube prefilteredMap;

        layout (binding = 6) uniform sampler2D albedoMap;
        layout (binding = 7) uniform sampler2D metallicMap;
        layout (binding = 8) uniform sampler2D roughnessMap;
        layout (binding = 9) uniform sampler2D normalMap;
        layout (binding = 10) uniform sampler2D aoMap;

        layout (binding = 11) uniform Parameters {
            vec4 albedo;
            float roughness;
            float metalness;
        } parameters;
    */
    void ForwardRenderer::BeginScene(const Camera& camera, const glm::mat4& viewMatrix)
    { /*
         auto& rapi = RenderAPI::Get();
         rapi.SetGraphicsPipeline(s_Data->Pipeline);
         s_Data->Mvp->Write(0, glm::value_ptr(camera.GetProjection()), sizeof(glm::mat4));
         s_Data->Mvp->Write(sizeof(glm::mat4), glm::value_ptr(viewMatrix), sizeof(glm::mat4));
         s_Data->Uniforms->SetTexture(0, 3, samplerIrradiance);
         s_Data->Uniforms->SetTexture(0, 4, samplerBRDFLUT);
         s_Data->Uniforms->SetTexture(0, 5, prefilteredMap);
         s_Data->Uniforms->SetTexture(0, 6, ImGuiMaterialPanel::GetSlectedMaterial()->GetAlbedoMap());
         s_Data->Uniforms->SetTexture(0, 7, ImGuiMaterialPanel::GetSlectedMaterial()->GetMetalnessMap());
         s_Data->Uniforms->SetTexture(0, 8, ImGuiMaterialPanel::GetSlectedMaterial()->GetRoughnessMap());
         s_Data->Uniforms->SetTexture(0, 9, ImGuiMaterialPanel::GetSlectedMaterial()->GetNormalMap());
         s_Data->Uniforms->SetTexture(0, 10, ImGuiMaterialPanel::GetSlectedMaterial()->GetAoMap());
         rapi.SetUniforms(s_Data->Uniforms);*/
    }

    void ForwardRenderer::SubmitLightSetup()
    {
        glm::vec3 lightPositions[] = {
            glm::vec3(-10.0f, 10.0f, 10.0f),
            glm::vec3(10.0f, 10.0f, 10.0f),
            glm::vec3(-10.0f, -10.0f, 10.0f),
            glm::vec3(10.0f, -10.0f, 10.0f),
        };
        glm::vec3 lightColors[] = { glm::vec3(300.0f, 300.0f, 300.0f), glm::vec3(300.0f, 300.0f, 300.0f),
                                    glm::vec3(300.0f, 300.0f, 300.0f), glm::vec3(300.0f, 300.0f, 300.0f) };

        ImGuiMaterialPanel::GetSlectedMaterial()->SetUniform("lightPositions", lightPositions);
        ImGuiMaterialPanel::GetSlectedMaterial()->SetUniform("lightColors", lightColors);
    }

    void ForwardRenderer::Submit(const Ref<Model>& model)
    {
        for (auto& mesh : model->GetMeshes())
            model->Draw();
    }

    void ForwardRenderer::SubmitMesh(const Ref<Mesh>& mesh, const glm::mat4& transform)
    {
        s_Data->Mvp->Write(sizeof(glm::mat4) * 2, glm::value_ptr(transform), sizeof(glm::mat4));
        SubmitLightSetup();
        mesh->Draw();
    }

    void ForwardRenderer::EndScene() {}

    void ForwardRenderer::End() {}

    void ForwardRenderer::Flush() {}

    void ForwardRenderer::Shutdown() { delete s_Data; }

} // namespace Crowny
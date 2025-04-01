#include "cwpch.h"

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
    extern float metalness;
    extern float roughness;
    extern glm::vec4 albedo;

    struct ForwardRendererData
    {
        Ref<Material> SkyboxMaterial;
        Ref<Material> PbrMaterial;
        Ref<Skybox> Skybox;
        Ref<VertexBuffer> SkyboxVbo;
        Ref<IndexBuffer> SkyboxIbo;
    };

    static ForwardRendererData* s_Data;

    float skyboxVertices2[] = { -1.0f, -1.0f, 1.0f,  1.0f, -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, -1.0f,
                      1.0f,  1.0f,  -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,
                      -1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f, -1.0f,
                      1.0f,  1.0f,  1.0f,  1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f, -1.0f };

    uint32_t skyboxIndices2[] = {
        0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11, 12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
    };

    void ForwardRenderer::Init()
    {
        s_Data = new ForwardRendererData();
        // Ref<UniformParams>& uniforms = InspectorPanel::GetSelectedMaterial()->GetUniformParams();

        // AssetHandle<Shader> shader1 = AssetManager::Get().Load<Shader>(PBRIBL_SHADER_PATH);
        Ref<Shader> pbriblShader = Importer::Get().Import<Shader>("Resources/Shaders/Pbribl.glsl");
        const AssetHandle<Shader> pbriblHandle = static_asset_cast<Shader>(AssetManager::Get().CreateAssetHandle(pbriblShader));
        s_Data->PbrMaterial = Material::Create(pbriblHandle);
        s_Data->Skybox = CreateRef<Skybox>("Resources/Textures/envmap.hdr");

        // TODO: Cube drawing code!!!!
        s_Data->SkyboxVbo = VertexBuffer::Create(skyboxVertices2, sizeof(skyboxVertices2));
        s_Data->SkyboxVbo->SetLayout(CreateRef<BufferLayout>(BufferLayout{ { ShaderDataType::Float3, "inPos" } }));
        s_Data->SkyboxIbo = IndexBuffer::Create(skyboxIndices2, sizeof(skyboxIndices2) / sizeof(uint32_t));

        // AssetHandle<Shader> shader = AssetManager::Get().Load<Shader>(SKYBOX_SHADER_PATH);
        Ref<Shader> skyboxShader = Importer::Get().Import<Shader>("Resources/Shaders/Skybox.glsl");
        const AssetHandle<Shader> skyboxHandle = static_asset_cast<Shader>(AssetManager::Get().CreateAssetHandle(skyboxShader));
        s_Data->SkyboxMaterial = Material::Create(skyboxHandle);
        s_Data->SkyboxMaterial->SetTexture("samplerEnv", s_Data->Skybox->m_EnvironmentMap);

        // Ref<Texture> albedo = Importer::Import()
        // Ref<Texture> diffuse = Importer::Get().Import<Texture>("C:\\dev\\Projects\\Project1\\Assets\\Models\\Achates\\textures\\mn_rpat_00_d.png");
        // Ref<Texture> normals = Importer::Get().Import<Texture>("C:\\dev\\Projects\\Project1\\Assets\\Models\\Achates\\textures\\mn_rpat_00_n.png");
        Ref<Texture> diffuse =
          Importer::Get().Import<Texture>("C:\\dev\\Projects\\Project1\\Assets\\Models\\b33-pollinator-robot\\textures\\Bee_low_03_-_Default_BaseColor.png");
        Ref<Texture> normals =
          Importer::Get().Import<Texture>("C:\\dev\\Projects\\Project1\\Assets\\Models\\b33-pollinator-robot\\textures\\Bee_low_03_-_Default_BaseColor.png");
        s_Data->PbrMaterial->SetTexture("albedoMap", diffuse);
        s_Data->PbrMaterial->SetTexture("metallicMap", Texture::WHITE);
        s_Data->PbrMaterial->SetTexture("roughnessMap", Texture::WHITE);
        s_Data->PbrMaterial->SetTexture("normalMap", normals);
        s_Data->PbrMaterial->SetTexture("aoMap", Texture::WHITE);

        s_Data->PbrMaterial->SetTexture("samplerIrradiance", s_Data->Skybox->m_IrradianceMap);
        s_Data->PbrMaterial->SetTexture("samplerBRDFLUT", s_Data->Skybox->m_Brdf);
        s_Data->PbrMaterial->SetTexture("prefilteredMap", s_Data->Skybox->m_PrefilteredMap);
        // s_Data->Test
    }

    void ForwardRenderer::Begin() {}

    void ForwardRenderer::BeginScene(const Camera& camera, const glm::mat4& viewMatrix)
    {
        auto& rapi = RenderAPI::Get();
        rapi.ClearViewport(FBT_COLOR | FBT_DEPTH);
        glm::mat4 inv = glm::mat4(glm::mat3(viewMatrix));
        s_Data->SkyboxMaterial->SetMatrix("mvp", camera.GetProjection() * inv);

        // glm::vec4 lightPositions[] = { glm::vec4(-10.0f, 10.0f, 10.0f, 1.0f), glm::vec4(10.0f, 10.0f, 10.0f, 1.0f),
        //                               // glm::vec4(-10.0f, -10.0f, 10.0f, 1.0f), glm::vec4(10.0f, -10.0f, 10.0f, 1.0f) };
        // glm::vec3 lightColors[] = { glm::vec//3(300.0f, 300.0f, 300.0f), glm::vec3(300.0f, 300.0f, 300.0f),
        //                             glm::vec3(300.0f, 300.0f, 300.0f), glm::vec3(300.0f, 300.0f, 300.0f) };
        float gamma = 2.2f, exposure = 4.5f;
        s_Data->SkyboxMaterial->SetFloat("gamma", gamma);
        s_Data->SkyboxMaterial->SetFloat("exposure", exposure);
        s_Data->PbrMaterial->SetFloat("gamma", gamma);
        s_Data->PbrMaterial->SetFloat("exposure", exposure);
        const glm::vec3 camPos = camera.GetPosition();
        s_Data->PbrMaterial->SetVector3("camPos", camPos);
        s_Data->PbrMaterial->SetMatrix("viewProjection", camera.GetProjection() * viewMatrix);
        
        rapi.SetGraphicsPipeline(s_Data->SkyboxMaterial->GetGraphicsPipeline());
        rapi.SetVertexBuffers(0, &s_Data->SkyboxVbo, 1);
        rapi.SetVertexLayout(s_Data->SkyboxVbo->GetLayout());
        rapi.SetIndexBuffer(s_Data->SkyboxIbo);
        rapi.SetUniforms(s_Data->SkyboxMaterial->GetUniformParams());
        rapi.DrawIndexed(0, s_Data->SkyboxIbo->GetCount(), 0, 1);
    }

    void ForwardRenderer::SubmitLightSetup() {}

    void ForwardRenderer::Submit(const AssetHandle<Mesh>& mesh, const glm::mat4& transform)
    {
        // Skybox b("Resources/Textures/envmap.hdr");
        RenderAPI& rapi = RenderAPI::Get();

        s_Data->PbrMaterial->SetColor("albedo", albedo);
        s_Data->PbrMaterial->SetFloat("roughness", roughness);
        s_Data->PbrMaterial->SetFloat("metalness", metalness);
        s_Data->PbrMaterial->SetMatrix("model", transform);

        rapi.SetGraphicsPipeline(s_Data->PbrMaterial->GetGraphicsPipeline());
        rapi.SetUniforms(s_Data->PbrMaterial->GetUniformParams());
        rapi.SetVertexLayout(mesh->GetVertexBuffer()->GetLayout());
        rapi.SetVertexBuffers(0, &mesh->GetVertexBuffer(), 1);
        rapi.SetIndexBuffer(mesh->GetIndexBuffer());
        rapi.SetDrawMode(mesh->GetDrawMode());
        rapi.DrawIndexed(0, mesh->GetIndexCount(), 0, mesh->GetVertexCount());
        // rapi.Draw(0, mesh->GetVertexCount());
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
        delete s_Data;
    }

} // namespace Crowny

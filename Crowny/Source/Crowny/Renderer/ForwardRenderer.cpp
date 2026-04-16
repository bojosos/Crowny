#include "cwpch.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/RenderAPI/GraphicsPipeline.h"
#include "Crowny/RenderAPI/RenderCommand.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/UniformBufferBlock.h"
#include "Crowny/RenderAPI/UniformParams.h"
#include "Crowny/RenderAPI/VertexArray.h"
#include "Crowny/RenderAPI/VertexBuffer.h"
#include "Crowny/Renderer/Camera.h"
#include "Crowny/Renderer/EnvironmentMap.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/ForwardRenderer.h"
#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Utils/ShaderCompiler.h"

#include <glm/gtc/type_ptr.hpp>
#include <tracy/Tracy.hpp>

#define renderstuff true

namespace Crowny
{
    float metalness = 0.0f;
    float roughness = 0.5f;
    glm::vec4 albedo = glm::vec4(1.0f);

    static const float s_SkyboxVertices[] = { -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,
                                              -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,
                                              1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,
                                              -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,
                                              -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f };

    static const uint32_t s_SkyboxIndices[] = {
        0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11, 12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
    };

    struct ForwardRendererData
    {
        Ref<Material> SkyboxMaterial;
        Ref<Material> PbrMaterial;
        Ref<Material> WireframeMaterial;
        Ref<VertexBuffer> SkyboxVbo;
        Ref<IndexBuffer> SkyboxIbo;

        // BRDF LUT — scene-independent, generated once
        Ref<Texture> BrdfLUT;

        // Default environment (fallback when scene has none)
        Ref<EnvironmentMap> DefaultEnvironment;

        // Per-frame state
        glm::mat4 ViewProjection;
        glm::vec3 CamPos;
        float Gamma = 2.2f;
        float Exposure = 4.5f;

        // Active environment for current frame
        Ref<Texture> ActiveIrradiance;
        Ref<Texture> ActivePrefiltered;
        Ref<Texture> ActiveCubemap;

        // Viewport polygon mode override (editor)
        PolygonMode OverridePolygonMode = PolygonMode::Solid;
    };

    static ForwardRendererData* s_Data;

    static void GenerateBRDFLUT()
    {
        auto& rapi = (*gRenderAPI);
        TextureDesc tProps;
        tProps.Width = 512;
        tProps.Height = 512;
        tProps.Format = TextureFormat::RG32F;
        tProps.Usage = TextureUsage::TEXTURE_RENDERTARGET;
        tProps.DebugName = "ForwardRenderer/BrdfLUT";
        s_Data->BrdfLUT = Texture::Create(tProps);

        RenderTextureDesc rtProps;
        rtProps.Width = tProps.Width;
        rtProps.Height = tProps.Height;
        rtProps.ColorSurfaces[0].Texture = s_Data->BrdfLUT;
        Ref<RenderTexture> target = RenderTexture::Create(rtProps);

        AssetHandle<Shader> shaderHandle = gAssetManager->Load<Shader>(BRDF_SHADER_PATH);
        Ref<Material> brdfMaterial = Material::Create(shaderHandle);
        rapi.SetRenderTarget(target);
        rapi.SetGraphicsPipeline(brdfMaterial->GetGraphicsPipeline());
        rapi.SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
        rapi.SetUniforms(brdfMaterial->GetUniformParams());
        rapi.Draw(0, 3, 1);
    }

    void ForwardRenderer::Init()
    {
#if !renderstuff
        return;
#endif
        s_Data = new ForwardRendererData();

        // Generate scene-independent BRDF LUT
        GenerateBRDFLUT();

        // Default PBR material
        AssetHandle<Shader> pbriblHandle = gAssetManager->Load<Shader>(PBRIBL_SHADER_PATH);
        s_Data->PbrMaterial = Material::CreatePBR(pbriblHandle);

        // Default environment
        s_Data->DefaultEnvironment = CreateRef<EnvironmentMap>("Resources/Textures/envmap.hdr");

        // Skybox mesh (single copy)
        s_Data->SkyboxVbo = VertexBuffer::Create({sizeof(s_SkyboxVertices), BufferUsage::BU_STATIC_DRAW, s_SkyboxVertices});
        s_Data->SkyboxVbo->SetLayout(CreateRef<BufferLayout>(BufferLayout{ { ShaderDataType::Float3, "inPos" } }));
        s_Data->SkyboxIbo = IndexBuffer::Create({36, IndexType::Index_32, BufferUsage::BU_STATIC_DRAW, s_SkyboxIndices});

        // Skybox material
        AssetHandle<Shader> skyboxHandle = gAssetManager->Load<Shader>(SKYBOX_SHADER_PATH);
        s_Data->SkyboxMaterial = Material::Create(skyboxHandle);

        // Wireframe material (editor overlay)
        {
            AssetHandle<Shader> wireframeShader = AssetManager::Get().Load<Shader>("Resources/Shaders/Wireframe.asset");
            s_Data->WireframeMaterial = Material::Create(wireframeShader);
        }
    }

    void ForwardRenderer::Begin() {}

    static void SetupSceneUniforms(const glm::mat4& projection, const glm::mat4& viewMatrix, const glm::vec3& cameraPosition)
    {
        s_Data->ViewProjection = projection * viewMatrix;
        s_Data->CamPos = cameraPosition;

        // Skybox
        glm::mat4 inv = glm::mat4(glm::mat3(viewMatrix));
        s_Data->SkyboxMaterial->SetMatrix("mvp", projection * inv);
        s_Data->SkyboxMaterial->SetFloat("gamma", s_Data->Gamma);
        s_Data->SkyboxMaterial->SetFloat("exposure", s_Data->Exposure);
    }

    static void BindEnvironment(const Ref<EnvironmentMap>& env)
    {
        if (env && env->IsValid())
        {
            s_Data->ActiveIrradiance = env->GetIrradianceMap();
            s_Data->ActivePrefiltered = env->GetPrefilteredMap();
            s_Data->ActiveCubemap = env->GetEnvironmentCubemap();
        }
        else if (s_Data->DefaultEnvironment && s_Data->DefaultEnvironment->IsValid())
        {
            s_Data->ActiveIrradiance = s_Data->DefaultEnvironment->GetIrradianceMap();
            s_Data->ActivePrefiltered = s_Data->DefaultEnvironment->GetPrefilteredMap();
            s_Data->ActiveCubemap = s_Data->DefaultEnvironment->GetEnvironmentCubemap();
        }
        else
        {
            s_Data->ActiveIrradiance = nullptr;
            s_Data->ActivePrefiltered = nullptr;
            s_Data->ActiveCubemap = nullptr;
        }

        // Bind cubemap to skybox material
        if (s_Data->ActiveCubemap)
            s_Data->SkyboxMaterial->SetTexture("cw_samplerEnv", s_Data->ActiveCubemap);
    }

    static void DrawSkybox(RenderAPI& rapi)
    {
        if (!s_Data->ActiveCubemap)
            return;
        rapi.SetGraphicsPipeline(s_Data->SkyboxMaterial->GetGraphicsPipeline());
        rapi.SetVertexBuffers(0, &s_Data->SkyboxVbo, 1);
        rapi.SetVertexLayout(s_Data->SkyboxVbo->GetLayout());
        rapi.SetIndexBuffer(s_Data->SkyboxIbo);
        rapi.SetUniforms(s_Data->SkyboxMaterial->GetUniformParams());
        rapi.DrawIndexed(0, s_Data->SkyboxIbo->GetCount(), 0, 1);
    }

    static void ApplySceneUniforms(const Ref<Material>& material, const glm::mat4& model)
    {
        material->SetMatrix("viewProjection", s_Data->ViewProjection);
        material->SetMatrix("model", model);
        material->SetVector3("camPos", s_Data->CamPos);
        material->SetFloat("gamma", s_Data->Gamma);
        material->SetFloat("exposure", s_Data->Exposure);

        // IBL textures
        if (material->HasBinding("cw_samplerIrradiance") && s_Data->ActiveIrradiance)
        {
            material->SetTexture("cw_samplerIrradiance", s_Data->ActiveIrradiance);
            material->SetTexture("cw_samplerBRDFLUT", s_Data->BrdfLUT);
            material->SetTexture("cw_prefilteredMap", s_Data->ActivePrefiltered);
        }
    }

    void ForwardRenderer::BeginScene(const Camera& camera, const glm::mat4& viewMatrix, const Ref<EnvironmentMap>& environment)
    {
        ZoneScopedN("ForwardRenderer::BeginScene");
        auto& rapi = (*gRenderAPI);
        rapi.ClearViewport(FBT_COLOR | FBT_DEPTH);
        BindEnvironment(environment);
        SetupSceneUniforms(camera.GetProjection(), viewMatrix, camera.GetPosition());
        DrawSkybox(rapi);
    }

    void ForwardRenderer::BeginScene(const glm::mat4& projection, const glm::mat4& viewMatrix, const glm::vec3& cameraPosition,
                                     const Ref<EnvironmentMap>& environment)
    {
        ZoneScopedN("ForwardRenderer::BeginScene");
        auto& rapi = (*gRenderAPI);
        rapi.ClearViewport(FBT_COLOR | FBT_DEPTH);
        BindEnvironment(environment);
        SetupSceneUniforms(projection, viewMatrix, cameraPosition);
        DrawSkybox(rapi);
    }

    void ForwardRenderer::SetPolygonMode(PolygonMode mode) { s_Data->OverridePolygonMode = mode; }

    void ForwardRenderer::SubmitLightSetup() {}

    static void DrawMaterialPasses(RenderAPI& rapi, const Ref<Material>& material, DrawMode drawMode, uint32_t indexOffset, uint32_t indexCount,
                                   uint32_t vertexCount)
    {
        for (uint32_t p = 0; p < material->GetPassCount(); p++)
        {
            rapi.SetGraphicsPipeline(material->GetGraphicsPipeline(p));
            rapi.SetUniforms(material->GetUniformParams(p));
            rapi.SetDrawMode(drawMode);
            rapi.DrawIndexed(indexOffset, indexCount, 0, vertexCount);
        }
    }

    void ForwardRenderer::Submit(const AssetHandle<Mesh>& mesh, const Vector<AssetHandle<Material>>& materials, const glm::mat4& transform)
    {
        ZoneScopedN("ForwardRenderer::Submit");
        RenderAPI& rapi = (*gRenderAPI);
        const Vector<SubMesh>& subMeshes = mesh->GetSubMeshes();

        auto getMaterial = [&](uint32_t index) -> Ref<Material> {
            if (index < materials.size() && materials[index])
                return materials[index].GetInternalPtr();
            if (!materials.empty() && materials[0])
                return materials[0].GetInternalPtr();
            return s_Data->PbrMaterial;
        };

        Ref<VertexBuffer> vb = mesh->GetVertexBuffer();
        rapi.SetVertexLayout(vb->GetLayout());
        rapi.SetVertexBuffers(0, &vb, 1);
        rapi.SetIndexBuffer(mesh->GetIndexBuffer());

        const bool wireframe = s_Data->OverridePolygonMode == PolygonMode::Wireframe;

        if (subMeshes.empty())
        {
            Ref<Material> renderMaterial = wireframe ? s_Data->WireframeMaterial : getMaterial(0);
            ApplySceneUniforms(renderMaterial, transform);
            if (!wireframe)
            {
                renderMaterial->SetColor("albedo", albedo);
                renderMaterial->SetFloat("roughness", roughness);
                renderMaterial->SetFloat("metalness", metalness);
            }
            DrawMaterialPasses(rapi, renderMaterial, mesh->GetDrawMode(), 0, mesh->GetIndexCount(), mesh->GetVertexCount());
        }
        else
        {
            for (uint32_t i = 0; i < (uint32_t)subMeshes.size(); i++)
            {
                const SubMesh& sub = subMeshes[i];
                Ref<Material> renderMaterial = wireframe ? s_Data->WireframeMaterial : getMaterial(i);
                ApplySceneUniforms(renderMaterial, transform);
                if (!wireframe)
                {
                    renderMaterial->SetColor("albedo", albedo);
                    renderMaterial->SetFloat("roughness", roughness);
                    renderMaterial->SetFloat("metalness", metalness);
                }
                DrawMaterialPasses(rapi, renderMaterial, sub.MeshDrawMode, sub.IndexOffset, sub.IndexCount, mesh->GetVertexCount());
            }
        }
    }

    void ForwardRenderer::SubmitMesh(const Ref<Mesh>& mesh, const glm::mat4& transform) {}

    void ForwardRenderer::EndScene() {}

    void ForwardRenderer::End() {}

    void ForwardRenderer::Flush() {}

    void ForwardRenderer::Shutdown() { delete s_Data; }

} // namespace Crowny

#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/FileSystem.h"
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
#include "Crowny/Renderer/GpuMaterial.h"
#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Utils/ShaderCompiler.h"

#include <glm/gtc/type_ptr.hpp>
#include <tracy/Tracy.hpp>

#define renderstuff true

namespace Crowny
{
    using namespace Literals;

    static constexpr const char* BRDF_TEXTURE_PATH = "Resources/Textures/Brdf.asset";

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
        std::array<glm::vec4, 4> LightPositionRange{};
        std::array<glm::vec4, 4> LightDirectionOuter{};
        std::array<glm::vec4, 4> LightColorIntensity{};
        std::array<glm::vec4, 4> LightSpotSourceBias{};
        std::array<glm::ivec4, 4> LightMetadata{};
        uint32_t LightCount = 0;

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
        auto& rapi = (*RenderAPI::TryGet());
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
        const Ref<RenderTexture> target = RenderTexture::Create(rtProps);

        const AssetHandle<Shader> shaderHandle = AssetManager::TryGet()->Load<Shader>(BRDF_SHADER_PATH);
        const Ref<Material> brdfMaterial = Material::Create(shaderHandle);
        rapi.SetRenderTarget(target);
        rapi.SetGraphicsPipeline(brdfMaterial->GetGraphicsPipeline());
        rapi.SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
        rapi.SetUniforms(brdfMaterial->GetUniformParams());
        rapi.Draw(0, 3, 1);
    }

    static Path GetBuiltInOutputPath(const Path& logicalPath)
    {
        const Path workingDirectory = Application::TryGet()->GetWorkingDirectory();
        if (fs::is_directory(workingDirectory / "Crowny-Editor/Resources"))
            return workingDirectory / "Crowny-Editor" / logicalPath;
        return workingDirectory / logicalPath;
    }

    void ForwardRenderer::Init()
    {
#if !renderstuff
        return;
#endif
        s_Data = new ForwardRendererData();

        if (FileSystem::FileExists(BRDF_TEXTURE_PATH))
        {
            const AssetHandle<Texture> brdfLut = AssetManager::TryGet()->Load<Texture>(BRDF_TEXTURE_PATH);
            s_Data->BrdfLUT = brdfLut ? brdfLut.GetInternalPtr() : nullptr;
        }
        if (!s_Data->BrdfLUT)
        {
            GenerateBRDFLUT();
#ifndef CW_DIST
            // This is a one-time development cook. Normal startup loads the
            // serialized texture from Builtin.cwpack without executing a pass.
            RenderAPI::TryGet()->SubmitCommandBuffer(nullptr, 0);
            AssetManager::TryGet()->Save(s_Data->BrdfLUT, GetBuiltInOutputPath(BRDF_TEXTURE_PATH));
#endif
        }

        // Default PBR material
        const AssetHandle<Shader> pbriblHandle = AssetManager::TryGet()->Load<Shader>(PBRIBL_SHADER_PATH);
        s_Data->PbrMaterial = Material::CreatePBR(pbriblHandle);

        // A scene environment is generated only when a scene assigns one. The
        // previous fallback decoded a 3200x1600 HDR and generated all IBL faces
        // and mips before an empty editor could present its first frame.
        s_Data->DefaultEnvironment = nullptr;

        // Skybox mesh (single copy)
        s_Data->SkyboxVbo = VertexBuffer::Create({sizeof(s_SkyboxVertices), BufferUsage::BU_STATIC_DRAW, s_SkyboxVertices});
        s_Data->SkyboxVbo->SetLayout(CreateRef<BufferLayout>(BufferLayout{ { ShaderDataType::Float3, "inPos" } }));
        s_Data->SkyboxIbo = IndexBuffer::Create({36, IndexType::Index_32, BufferUsage::BU_STATIC_DRAW, s_SkyboxIndices});

        // Skybox material
        const AssetHandle<Shader> skyboxHandle = AssetManager::TryGet()->Load<Shader>(SKYBOX_SHADER_PATH);
        s_Data->SkyboxMaterial = Material::Create(skyboxHandle);

        // Wireframe material (editor overlay)
        {
            const AssetHandle<Shader> wireframeShader = AssetManager::Get().Load<Shader>("Resources/Shaders/Wireframe.asset");
            s_Data->WireframeMaterial = Material::Create(wireframeShader);
        }
    }

    void ForwardRenderer::Begin() {}

    static void SetupSceneUniforms(const glm::mat4& projection, const glm::mat4& viewMatrix, const glm::vec3& cameraPosition)
    {
        s_Data->ViewProjection = projection * viewMatrix;
        s_Data->CamPos = cameraPosition;

        // Skybox
        const glm::mat4 inv = glm::mat4(glm::mat3(viewMatrix));
        s_Data->SkyboxMaterial->SetMatrix("mvp"_hstr, projection * inv);
        s_Data->SkyboxMaterial->SetFloat("gamma"_hstr, s_Data->Gamma);
        s_Data->SkyboxMaterial->SetFloat("exposure"_hstr, s_Data->Exposure);
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
            s_Data->SkyboxMaterial->SetTexture("cw_samplerEnv"_hstr, s_Data->ActiveCubemap);
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
        material->SetMatrix("viewProjection"_hstr, s_Data->ViewProjection);
        material->SetMatrix("model"_hstr, model);
        material->SetVector3("camPos"_hstr, s_Data->CamPos);
        material->SetFloat("gamma"_hstr, s_Data->Gamma);
        material->SetFloat("exposure"_hstr, s_Data->Exposure);
        material->SetInt("lightCount"_hstr, static_cast<int32_t>(s_Data->LightCount));
        material->SetVector4Array("lightPositionRange", s_Data->LightPositionRange.data(), s_Data->LightPositionRange.size());
        material->SetVector4Array("lightDirectionOuter", s_Data->LightDirectionOuter.data(), s_Data->LightDirectionOuter.size());
        material->SetVector4Array("lightColorIntensity", s_Data->LightColorIntensity.data(), s_Data->LightColorIntensity.size());
        material->SetVector4Array("lightSpotSourceBias", s_Data->LightSpotSourceBias.data(), s_Data->LightSpotSourceBias.size());
        material->SetInt4Array("lightMetadata", s_Data->LightMetadata.data(), s_Data->LightMetadata.size());
        if (material->HasBinding("useIBL"_hstr))
            material->SetFloat("useIBL"_hstr, s_Data->ActiveIrradiance && s_Data->ActivePrefiltered ? 1.0f : 0.0f);

        // IBL textures
        if (material->HasBinding("cw_samplerIrradiance"_hstr) && s_Data->ActiveIrradiance)
        {
            material->SetTexture("cw_samplerIrradiance"_hstr, s_Data->ActiveIrradiance);
            material->SetTexture("cw_samplerBRDFLUT"_hstr, s_Data->BrdfLUT);
            material->SetTexture("cw_prefilteredMap"_hstr, s_Data->ActivePrefiltered);
        }
    }

    void ForwardRenderer::BeginScene(const Camera& camera, const glm::mat4& viewMatrix, const Ref<EnvironmentMap>& environment)
    {
        ZoneScopedN("ForwardRenderer::BeginScene");
        auto& rapi = (*RenderAPI::TryGet());
        rapi.ClearViewport(FBT_COLOR | FBT_DEPTH);
        BindEnvironment(environment);
        SetupSceneUniforms(camera.GetProjection(), viewMatrix, camera.GetPosition());
        DrawSkybox(rapi);
    }

    void ForwardRenderer::BeginScene(const glm::mat4& projection, const glm::mat4& viewMatrix, const glm::vec3& cameraPosition,
                                     const Ref<EnvironmentMap>& environment)
    {
        ZoneScopedN("ForwardRenderer::BeginScene");
        auto& rapi = (*RenderAPI::TryGet());
        rapi.ClearViewport(FBT_COLOR | FBT_DEPTH);
        BindEnvironment(environment);
        SetupSceneUniforms(projection, viewMatrix, cameraPosition);
        DrawSkybox(rapi);
    }

    void ForwardRenderer::BeginForwardOnlyScene(const glm::mat4& projection, const glm::mat4& viewMatrix,
                                                 const glm::vec3& cameraPosition, const Ref<EnvironmentMap>& environment)
    {
        ZoneScopedN("ForwardRenderer::BeginForwardOnlyScene");
        BindEnvironment(environment);
        SetupSceneUniforms(projection, viewMatrix, cameraPosition);
    }

    void ForwardRenderer::SetPolygonMode(PolygonMode mode) { s_Data->OverridePolygonMode = mode; }

    void ForwardRenderer::SubmitLightSetup() {}

    void ForwardRenderer::SetLights(const RenderLightData* lights, uint32_t lightCount)
    {
        std::array<const RenderLightData*, 4> selected{};
        std::array<float, 4> scores{ -1.0f, -1.0f, -1.0f, -1.0f };
        uint32_t selectedCount = 0;

        for (uint32_t lightIndex = 0; lights != nullptr && lightIndex < lightCount; lightIndex++)
        {
            const RenderLightData& light = lights[lightIndex];
            const RenderLightFlags flags = static_cast<RenderLightFlags>(light.Metadata.y);
            if (!HasFlag(flags, RenderLightFlags::Enabled))
                continue;

            const LightType type = static_cast<LightType>(light.Metadata.x);
            const float distanceSquared = glm::length2(glm::vec3(light.PositionRange) - s_Data->CamPos);
            float score = type == LightType::Directional ? std::numeric_limits<float>::max()
                                                         : light.ColorIntensity.w / std::max(distanceSquared, 0.01f);
            if (HasFlag(flags, RenderLightFlags::CastShadows) && std::isfinite(score))
                score *= 2.0f;

            uint32_t insertAt = selectedCount;
            while (insertAt > 0 && score > scores[insertAt - 1u])
                insertAt--;
            if (insertAt >= selected.size())
                continue;
            const uint32_t last = std::min<uint32_t>(selectedCount, selected.size() - 1u);
            for (uint32_t move = last; move > insertAt; move--)
            {
                selected[move] = selected[move - 1u];
                scores[move] = scores[move - 1u];
            }
            selected[insertAt] = &light;
            scores[insertAt] = score;
            selectedCount = std::min<uint32_t>(selectedCount + 1u, selected.size());
        }

        s_Data->LightCount = selectedCount;
        for (uint32_t index = 0; index < s_Data->LightPositionRange.size(); index++)
        {
            const RenderLightData light = index < selectedCount ? *selected[index] : RenderLightData{};
            s_Data->LightPositionRange[index] = light.PositionRange;
            s_Data->LightDirectionOuter[index] = light.DirectionOuterCosine;
            s_Data->LightColorIntensity[index] = light.ColorIntensity;
            s_Data->LightSpotSourceBias[index] = light.SpotSourceAndBias;
            s_Data->LightMetadata[index] = glm::ivec4(light.Metadata);
        }
    }

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

    void ForwardRenderer::Submit(const AssetHandle<Mesh>& mesh, std::span<const AssetHandle<Material>> materials, const glm::mat4& transform)
    {
        ZoneScopedN("ForwardRenderer::Submit");
        RenderAPI& rapi = (*RenderAPI::TryGet());
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
            const Ref<Material> renderMaterial = wireframe ? s_Data->WireframeMaterial : getMaterial(0);
            ApplySceneUniforms(renderMaterial, transform);
            if (!wireframe)
            {
                renderMaterial->SetColor("albedo"_hstr, albedo);
                renderMaterial->SetFloat("roughness"_hstr, roughness);
                renderMaterial->SetFloat("metalness"_hstr, metalness);
            }
            DrawMaterialPasses(rapi, renderMaterial, mesh->GetDrawMode(), 0, mesh->GetIndexCount(), mesh->GetVertexCount());
        }
        else
        {
            for (uint32_t i = 0; i < (uint32_t)subMeshes.size(); i++)
            {
                const SubMesh& sub = subMeshes[i];
                const Ref<Material> renderMaterial = wireframe ? s_Data->WireframeMaterial : getMaterial(i);
                ApplySceneUniforms(renderMaterial, transform);
                if (!wireframe)
                {
                    renderMaterial->SetColor("albedo"_hstr, albedo);
                    renderMaterial->SetFloat("roughness"_hstr, roughness);
                    renderMaterial->SetFloat("metalness"_hstr, metalness);
                }
                DrawMaterialPasses(rapi, renderMaterial, sub.MeshDrawMode, sub.IndexOffset, sub.IndexCount, mesh->GetVertexCount());
            }
        }
    }

    void ForwardRenderer::SubmitForwardOnlyOpaque(const AssetHandle<Mesh>& mesh, std::span<const AssetHandle<Material>> materials,
                                                   const glm::mat4& transform)
    {
        ZoneScopedN("ForwardRenderer::SubmitForwardOnlyOpaque");
        if (!mesh)
            return;

        RenderAPI& rapi = *RenderAPI::TryGet();
        const Vector<SubMesh>& subMeshes = mesh->GetSubMeshes();
        auto getMaterial = [&](uint32_t index) -> Ref<Material> {
            if (index < materials.size() && materials[index])
                return materials[index].GetInternalPtr();
            if (!materials.empty() && materials[0])
                return materials[0].GetInternalPtr();
            return s_Data->PbrMaterial;
        };
        auto draw = [&](uint32_t materialIndex, DrawMode drawMode, uint32_t indexOffset, uint32_t indexCount) {
            const Ref<Material> sourceMaterial = getMaterial(materialIndex);
            if (!sourceMaterial || !MaterialRenderClassifier::Classify(*sourceMaterial).IsForwardOnlyOpaque())
                return;
            const Ref<Material> renderMaterial =
              s_Data->OverridePolygonMode == PolygonMode::Wireframe ? s_Data->WireframeMaterial : sourceMaterial;
            ApplySceneUniforms(renderMaterial, transform);
            DrawMaterialPasses(rapi, renderMaterial, drawMode, indexOffset, indexCount, mesh->GetVertexCount());
        };

        Ref<VertexBuffer> vertexBuffer = mesh->GetVertexBuffer();
        if (!vertexBuffer || !mesh->GetIndexBuffer())
            return;
        rapi.SetVertexLayout(vertexBuffer->GetLayout());
        rapi.SetVertexBuffers(0, &vertexBuffer, 1);
        rapi.SetIndexBuffer(mesh->GetIndexBuffer());

        if (subMeshes.empty())
            draw(0, mesh->GetDrawMode(), 0, mesh->GetIndexCount());
        else
        {
            for (uint32_t index = 0; index < static_cast<uint32_t>(subMeshes.size()); index++)
            {
                const SubMesh& subMesh = subMeshes[index];
                draw(index, subMesh.MeshDrawMode, subMesh.IndexOffset, subMesh.IndexCount);
            }
        }
    }

    void ForwardRenderer::SubmitMesh(const Ref<Mesh>& mesh, const glm::mat4& transform) {}

    void ForwardRenderer::EndScene() {}

    void ForwardRenderer::End() {}

    void ForwardRenderer::Flush() {}

    void ForwardRenderer::Shutdown()
    {
        delete s_Data;
        s_Data = nullptr;
    }

} // namespace Crowny

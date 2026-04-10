#include "cwpch.h"

#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scene/SceneRenderer.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/RenderAPI/AccelerationStructure.h"
#include "Crowny/RenderAPI/IndexBuffer.h"
#include "Crowny/RenderAPI/Query.h"
#include "Crowny/RenderAPI/RenderCommand.h"
#include "Crowny/RenderAPI/VertexBuffer.h"
#include "Crowny/Renderer/ForwardRenderer.h"
#include "Crowny/Renderer/Renderer2D.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Import/Importer.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

#define TRACY_ENABLE
#include <tracy/Tracy.hpp>

#define raytracing false

namespace Crowny
{

    struct SceneRendererData
    {
        uint32_t ViewportWidth, ViewportHeight;

        // Ref<TimerQuery> Timer2DGeometry = nullptr;
        // Ref<TimerQuery> Timer3DGeometry = nullptr;

        // Ref<PipelineQuery> PipelineQuery = nullptr;

        // Ref<RayTracingPipeline> RayPipeline = nullptr;
        // Ref<AccelerationStructure> Accel = nullptr;

        // AssetHandle<Font> GlobalFont;
    };

    struct SceneRendererStats
    {
        uint32_t Vertices;
        uint32_t Triangles;

        float FrameTime;
        float Frames;
    };

    static SceneRendererData* s_Data;
    static SceneRendererStats s_Stats;

    SceneRenderer::SceneRenderer(const Ref<Scene>& scene, const Ref<RenderTarget>& renderTarget) : m_Scene(scene), m_RenderTarget(renderTarget) {}

    void SceneRenderer::Init()
    {
        s_Data = new SceneRendererData();
        // s_Data->Timer2DGeometry = TimerQuery::Create();
        // s_Data->Timer3DGeometry = TimerQuery::Create();

        // s_Data->PipelineQuery = PipelineQuery::Create();

#if raytracing
        static Ref<Shader> rayTraceShader = Importer::Get().Import<Shader>("Resources/Shaders/RayTrace.glsl");
        static const AssetHandle<Shader> rayTraceHandle = static_asset_cast<Shader>(AssetManager::Get().CreateAssetHandle(rayTraceShader));
        rayTraceShader->GetTechniques()[0]->GetRenderPasses()[0]->Compile();

        float vertices[] = { 1.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f };
        uint32_t indices[] = { 0, 1, 2 };
        const glm::mat3x4 transformMatrix(1.0f);
        Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create(vertices, sizeof(vertices));
        Ref<IndexBuffer> indexBuffer = IndexBuffer::Create(indices, 3);

        s_Data->RayPipeline = rayTraceShader->GetTechniques()[0]->GetRenderPasses()[0]->GetRayTracingPipeline();
        AccelerationGeometry geom;
        geom.Transform = transformMatrix;
        geom.UseTransform = true;
        geom.Type = GeometryType::Triangles;
        GeometryTriangles tris;
        tris.VertexBuffer = vertexBuffer;
        tris.IndexBuffer = indexBuffer;
        tris.IndexCount = 3;
        tris.VertexCount = 9;
        tris.IndexFormat = IndexType::Index_32;
        tris.VertexStride = 12;
        geom.GeometryData.Triangles = tris;

        static Ref<AccelerationStructure> blas = AccelerationStructure::Create({ geom }, false, 1, AccelerationStructBuildBits::PreferFastTrace);
        AccelerationInstance instance;
        instance.BottomLevelAccel = blas.get();
        instance.Transform = transformMatrix;
        s_Data->Accel = AccelerationStructure::Create({}, true, 1, AccelerationStructBuildBits::PreferFastTrace);

        Ref<CommandBuffer> cmdBuf = CommandBuffer::Create(GpuQueueType::GRAPHICS_QUEUE);
        blas->BuildBottomLevel(cmdBuf, &geom, 1, AccelerationStructBuildBits::PreferFastTrace);
        s_Data->Accel->BuildTopLevel(cmdBuf, &instance, 1, AccelerationStructBuildBits::PreferFastTrace);
        RenderAPI::Get().SubmitCommandBuffer(cmdBuf, 0);
#endif

        // Ref<Asset> font = Importer::Get().Import("Resources/Fonts/Roboto/roboto-thin.ttf");
        // s_Data.GlobalFont = static_asset_cast<Font>(AssetManager::Get().CreateAssetHandle(font));
    }

    void SceneRenderer::RenderEditor(const EditorCamera& camera) { Render(camera, camera.GetViewMatrix()); }

    void SceneRenderer::Render()
    {
        // Get the main camera to render from the scene
        Camera* mainCamera = nullptr;
        glm::mat4 cameraTransform;
        const auto cameraView = m_Scene->m_Registry.view<TransformComponent, CameraComponent>();
        for (const entt::entity ee : cameraView)
        {
            auto [transform, camera] = cameraView.get<TransformComponent, CameraComponent>(ee);
            mainCamera = &camera.Camera;
            Entity entity(ee, m_Scene.get());
            cameraTransform = entity.GetWorldMatrix();
            break;
        }

        // Render the scene
        if (mainCamera)
            Render(*mainCamera, glm::inverse(cameraTransform));
    }

    void SceneRenderer::Render(const Camera& camera, const glm::mat4& viewTransform)
    {
        FrameMarkStart("Editor update");
        RenderAPI& rapi = RenderAPI::Get();
        rapi.SetRenderTarget(m_RenderTarget);
        rapi.SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
        rapi.ClearRenderTarget(FBT_COLOR | FBT_DEPTH);
#if raytracing
        rapi.SetRayTracingPipeline(s_Data->RayPipeline);
        rapi.TraceRays(s_Data->ViewportWidth, s_Data->ViewportHeight);

        return;
#endif
        {
            ZoneScopedN("Forward begin");
            // TODO: Combine these, or name them better
            ForwardRenderer::Begin();
            ForwardRenderer::BeginScene(camera, viewTransform);
        }
        {
            ZoneScopedN("Forward render");
            auto objs = m_Scene->m_Registry.group<MeshRendererComponent>(entt::get<TransformComponent>);
            for (const entt::entity ee : objs)
            {
                auto [transform, mesh] = m_Scene->m_Registry.get<TransformComponent, MeshRendererComponent>(ee);

                if (mesh.MeshHandle)
                {
                    Entity entity(ee, m_Scene.get());
                    ForwardRenderer::Submit(mesh.MeshHandle, mesh.Materials, entity.GetWorldMatrix());
                    // TODO: Update stats... triangle count has to take into account the draw mode
                }
            }
        }
        {
            ZoneScopedN("Forward end");
            ForwardRenderer::Flush();
            ForwardRenderer::EndScene();
            ForwardRenderer::End();
        }

        {
            ZoneScopedN("2D render");
            Renderer2D::Begin(camera, viewTransform);
            const auto spriteRendererComponents = m_Scene->m_Registry.group<SpriteRendererComponent>(entt::get<TransformComponent>);
            for (const entt::entity ee : spriteRendererComponents)
            {
                auto [transform, sprite] = m_Scene->m_Registry.get<TransformComponent, SpriteRendererComponent>(ee);
                Entity entity(ee, m_Scene.get());
                CW_ENGINE_ASSERT(entity.IsValid());
                Renderer2D::FillRect(entity.GetWorldMatrix(), sprite.Texture ? sprite.Texture.GetInternalPtr() : nullptr, sprite.Color,
                                     ((int32_t)ee) + 1);
                // Renderer2D::FillRect(glm::mat4(1.0f), sprite.Texture, sprite.Color, ((int32_t)ee) + 1);
                s_Stats.Vertices += 6;
                s_Stats.Triangles += 2;
            }
            const auto textComponents = m_Scene->m_Registry.group<TextComponent>(entt::get<TransformComponent>);
            for (const entt::entity ee : textComponents)
            {
                auto [transform, text] = m_Scene->m_Registry.get<TransformComponent, TextComponent>(ee);
                Entity entity(ee, m_Scene.get());
                CW_ENGINE_ASSERT(entity.IsValid());
                Renderer2D::DrawString(text, entity.GetWorldMatrix(), (int32_t)ee + 1);
                s_Stats.Vertices += (uint32_t)text.Text.size() * 6;
                s_Stats.Triangles += (uint32_t)text.Text.size() * 2;
            }
            Renderer2D::End();
        }

        s_Stats.Frames += 1;
        // s_Stats.FrameTime = ts;
        FrameMarkEnd("Editor update");
    }

    void SceneRenderer::SetRenderTarget(const Ref<RenderTarget>& renderTarget) { m_RenderTarget = renderTarget; }

    void SceneRenderer::SetScene(const Ref<Scene>& scene) { m_Scene = scene; }

} // namespace Crowny

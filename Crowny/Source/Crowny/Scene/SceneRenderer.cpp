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

namespace Crowny
{

    struct SceneRendererData
    {
        uint32_t ViewportWidth, ViewportHeight;

        Ref<TimerQuery> Timer2DGeometry = nullptr;
        Ref<TimerQuery> Timer3DGeometry = nullptr;

        Ref<PipelineQuery> PipelineQuery = nullptr;

        Ref<RayTracingPipeline> RayPipeline = nullptr;
        Ref<AccelerationStructure> Accel = nullptr;

        AssetHandle<Font> GlobalFont;
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

    void SceneRenderer::Init()
    {
        s_Data = new SceneRendererData();
        s_Data->Timer2DGeometry = TimerQuery::Create();
        s_Data->Timer3DGeometry = TimerQuery::Create();

        s_Data->PipelineQuery = PipelineQuery::Create();

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

        Ref<CommandBuffer> cmdBuf = CommandBuffer::Create(GpuQueueType::COMPUTE_QUEUE);
        blas->BuildBottomLevel(cmdBuf, &geom, 1, AccelerationStructBuildBits::PreferFastTrace);
        s_Data->Accel->BuildTopLevel(cmdBuf, &instance, 1, AccelerationStructBuildBits::PreferFastTrace);
        RenderAPI::Get().SubmitCommandBuffer(cmdBuf, 0);

        // Ref<Asset> font = Importer::Get().Import("Resources/Fonts/Roboto/roboto-thin.ttf");
        // s_Data.GlobalFont = static_asset_cast<Font>(AssetManager::Get().CreateAssetHandle(font));
    }

    void SceneRenderer::OnEditorUpdate(Timestep ts, const EditorCamera& camera)
    {
        FrameMarkStart("Editor update");
        Ref<Scene> scene = SceneManager::GetActiveScene();

        // s_Data.PipelineQuery->Begin();
        // s_Data.Timer3DGeometry->Begin();
        // s_Data.Timer3DGeometry->End();

        // s_Data.Timer2DGeometry->Begin();
        RenderAPI& rapi = RenderAPI::Get();
        rapi.SetRayTracingPipeline(s_Data->RayPipeline);
        rapi.TraceRays(s_Data->ViewportWidth, s_Data->ViewportHeight);

        return;

        {
            ZoneScopedN("Forward begin");
            // TODO: Combine these, or name them better
            ForwardRenderer::Begin();
            ForwardRenderer::BeginScene(camera, camera.GetViewMatrix());
        }
        {
            ZoneScopedN("Forward render");
            auto objs = scene->m_Registry.group<MeshRendererComponent>(entt::get<TransformComponent>);
            for (const entt::entity ee : objs)
            {
                auto [transform, mesh] = scene->m_Registry.get<TransformComponent, MeshRendererComponent>(ee);

                if (mesh.MeshHandle)
                {
                    Entity entity(ee, scene.get());
                    ForwardRenderer::Submit(mesh.MeshHandle, entity.GetWorldMatrix());
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
            Renderer2D::Begin(camera, camera.GetViewMatrix());
            const auto spriteRendererComponents = scene->m_Registry.group<SpriteRendererComponent>(entt::get<TransformComponent>);
            for (const entt::entity ee : spriteRendererComponents)
            {
                auto [transform, sprite] = scene->m_Registry.get<TransformComponent, SpriteRendererComponent>(ee);
                Entity entity(ee, scene.get());
                CW_ENGINE_ASSERT(entity.IsValid());
                Renderer2D::FillRect(entity.GetWorldMatrix(), sprite.Texture ? sprite.Texture.GetInternalPtr() : nullptr, sprite.Color,
                                     ((int32_t)ee) + 1);
                // Renderer2D::FillRect(glm::mat4(1.0f), sprite.Texture, sprite.Color, ((int32_t)ee) + 1);
                s_Stats.Vertices += 6;
                s_Stats.Triangles += 2;
            }
            const auto textComponents = scene->m_Registry.group<TextComponent>(entt::get<TransformComponent>);
            for (const entt::entity ee : textComponents)
            {
                auto [transform, text] = scene->m_Registry.get<TransformComponent, TextComponent>(ee);
                Entity entity(ee, scene.get());
                CW_ENGINE_ASSERT(entity.IsValid());
                Renderer2D::DrawString(text, entity.GetWorldMatrix(), (int32_t)ee + 1);
                s_Stats.Vertices += (uint32_t)text.Text.size() * 6;
                s_Stats.Triangles += (uint32_t)text.Text.size() * 2;
            }
            Renderer2D::End();
        }
        // s_Data.Timer2DGeometry->End();
        // s_Data.PipelineQuery->End();

        // s_Stats.FrameTime = s_Data.Timer3DGeometry->GetTimeMs() + s_Data.Timer2DGeometry->GetTimeMs();

        s_Stats.Frames += 1;
        s_Stats.FrameTime = ts;
        FrameMarkEnd("Editor update");
    }

    void SceneRenderer::OnRuntimeUpdate(Timestep ts)
    {
        Ref<Scene> scene = SceneManager::GetActiveScene();

        // Get the main camera to render from
        Camera* mainCamera = nullptr;
        glm::mat4 cameraTransform;
        const auto cameraView = scene->m_Registry.view<TransformComponent, CameraComponent>();
        for (const entt::entity ee : cameraView)
        {
            auto [transform, camera] = cameraView.get<TransformComponent, CameraComponent>(ee);
            mainCamera = &camera.Camera;
            Entity entity(ee, scene.get());
            cameraTransform = entity.GetWorldMatrix();
            break;
        }

        // Render the scene
        if (mainCamera)
        {
            ForwardRenderer::Begin();
            ForwardRenderer::BeginScene(*mainCamera, glm::inverse(cameraTransform));
            auto objs = scene->m_Registry.group<MeshRendererComponent>(entt::get<TransformComponent>);
            for (const entt::entity ee : objs)
            {
                auto [transform, mesh] = scene->m_Registry.get<TransformComponent, MeshRendererComponent>(ee);

                if (mesh.MeshHandle)
                {
                    Entity entity(ee, scene.get());
                    ForwardRenderer::Submit(mesh.MeshHandle, entity.GetWorldMatrix());
                    // TODO: Update stats... triangle count has to take into account the draw mode
                }
            }
            ForwardRenderer::Flush();
            ForwardRenderer::EndScene();
            ForwardRenderer::End();

            Renderer2D::Begin(*mainCamera, glm::inverse(cameraTransform));
            const auto group = scene->m_Registry.group<SpriteRendererComponent>(entt::get<TransformComponent>);
            for (const entt::entity ee : group)
            {
                const auto [transform, sprite] = scene->m_Registry.get<TransformComponent, SpriteRendererComponent>(ee);
                Entity entity(ee, scene.get());
                Renderer2D::FillRect(entity.GetWorldMatrix(), sprite.Texture ? sprite.Texture.GetInternalPtr() : nullptr, sprite.Color, (uint32_t)ee);
                s_Stats.Vertices += 4;
                s_Stats.Triangles += 2;
            }
            const auto texts = scene->m_Registry.group<TextComponent>(entt::get<TransformComponent>);
            for (const auto ee : texts)
            {
                const auto [transform, text] = scene->m_Registry.get<TransformComponent, TextComponent>(ee);
                Entity entity(ee, scene.get());
                Renderer2D::DrawString(text, entity.GetWorldMatrix(), (int32_t)ee + 1);
                s_Stats.Vertices += (uint32_t)text.Text.size() * 4;
                s_Stats.Triangles += (uint32_t)text.Text.size() * 2;
            }
            Renderer2D::End();
        }
    }

    void SceneRenderer::SetViewportSize(float width, float height)
    {
        s_Data->ViewportWidth = (uint32_t)width;
        s_Data->ViewportHeight = (uint32_t)height;
    }

    void SceneRenderer::Shutdown()
    {
        s_Data->PipelineQuery = nullptr;
        s_Data->Timer2DGeometry = nullptr;
        s_Data->Timer3DGeometry = nullptr;
        delete s_Data;
    }

} // namespace Crowny

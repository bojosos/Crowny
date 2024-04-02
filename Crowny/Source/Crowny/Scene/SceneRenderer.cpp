#include "cwpch.h"

#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scene/SceneRenderer.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/RenderAPI/Query.h"
#include "Crowny/RenderAPI/RenderCommand.h"
#include "Crowny/Renderer/ForwardRenderer.h"
#include "Crowny/Renderer/Renderer2D.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Import/Importer.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

namespace Crowny
{

    struct SceneRendererData
    {
        uint32_t ViewportWidth, ViewportHeight;

        Ref<TimerQuery> Timer2DGeometry = nullptr;
        Ref<TimerQuery> Timer3DGeometry = nullptr;

        Ref<PipelineQuery> PipelineQuery = nullptr;

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
        // Ref<Asset> font = Importer::Get().Import("Resources/Fonts/Roboto/roboto-thin.ttf");
        // s_Data.GlobalFont = static_asset_cast<Font>(AssetManager::Get().CreateAssetHandle(font));
    }

    void SceneRenderer::OnEditorUpdate(Timestep ts, const EditorCamera& camera)
    {
        Ref<Scene> scene = SceneManager::GetActiveScene();

        // s_Data.PipelineQuery->Begin();
        // s_Data.Timer3DGeometry->Begin();
        // s_Data.Timer3DGeometry->End();

        // s_Data.Timer2DGeometry->Begin();

        ForwardRenderer::Begin();
        ForwardRenderer::BeginScene(camera, camera.GetViewMatrix());
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

        Renderer2D::Begin(camera, camera.GetViewMatrix());
        const auto sprineRendererComponents =
          scene->m_Registry.group<SpriteRendererComponent>(entt::get<TransformComponent>);
        for (const entt::entity ee : sprineRendererComponents)
        {
            auto [transform, sprite] = scene->m_Registry.get<TransformComponent, SpriteRendererComponent>(ee);
            Entity entity(ee, scene.get());
            CW_ENGINE_ASSERT(entity.IsValid());
            Renderer2D::FillRect(entity.GetWorldMatrix(), sprite.Texture, sprite.Color, ((int32_t)ee) + 1);
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
        // s_Data.Timer2DGeometry->End();
        // s_Data.PipelineQuery->End();

        // s_Stats.FrameTime = s_Data.Timer3DGeometry->GetTimeMs() + s_Data.Timer2DGeometry->GetTimeMs();

        s_Stats.Frames += 1;
        s_Stats.FrameTime = ts;
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
                Renderer2D::FillRect(entity.GetWorldMatrix(), sprite.Texture, sprite.Color, (uint32_t)ee);
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

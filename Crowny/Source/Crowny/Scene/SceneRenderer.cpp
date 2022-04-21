#include "cwpch.h"

#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scene/SceneRenderer.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/RenderAPI/Query.h"
#include "Crowny/RenderAPI/RenderCommand.h"
#include "Crowny/Renderer/ForwardRenderer.h"
#include "Crowny/Renderer/Renderer2D.h"

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
    };

    struct SceneRendererStats
    {
        uint32_t Vertices;
        uint32_t Triangles;

        float FrameTime;
        float Frames;
    };

    static SceneRendererData s_Data;
    static SceneRendererStats s_Stats;

    void SceneRenderer::Init()
    {
		s_Data.Timer2DGeometry = TimerQuery::Create();
		s_Data.Timer3DGeometry = TimerQuery::Create();

		s_Data.PipelineQuery = PipelineQuery::Create();
    }

    void SceneRenderer::OnEditorUpdate(Timestep ts, const EditorCamera& camera)
    {
        Ref<Scene> scene = SceneManager::GetActiveScene();

		// s_Data.PipelineQuery->Begin();
        // s_Data.Timer3DGeometry->Begin();
		
		/*
        ForwardRenderer::Begin();
        ForwardRenderer::BeginScene(camera, camera.GetViewMatrix());
        auto objs = scene->m_Registry.group<MeshRendererComponent>(entt::get<TransformComponent>);
        for (auto obj : objs)
        {
            auto [transform, mesh] = scene->m_Registry.get<TransformComponent, MeshRendererComponent>(obj);
            //if (mesh.Model)
            //{
            //    ForwardRenderer::Submit(mesh.Model, transform.GetTransform());
            //    // TODO: Update stats... triangle count has to take into account the draw mode
            //}
        }
        ForwardRenderer::Flush();
        ForwardRenderer::EndScene();
        ForwardRenderer::End();*/
        // s_Data.Timer3DGeometry->End();

        // s_Data.Timer2DGeometry->Begin();
        Renderer2D::Begin(camera, camera.GetViewMatrix());
        auto group = scene->m_Registry.group<SpriteRendererComponent>(entt::get<TransformComponent>);
        for (auto ee : group)
        {
            auto [transform, sprite] = scene->m_Registry.get<TransformComponent, SpriteRendererComponent>(ee);
            Renderer2D::FillRect(transform.GetTransform(), sprite.Texture, sprite.Color, ((uint32_t)ee) + 1);
            s_Stats.Vertices += 6;
            s_Stats.Triangles += 2;
        }
        auto texts = scene->m_Registry.group<TextComponent>(entt::get<TransformComponent>);
        for (auto ee : texts)
        {
            auto [transform, text] = scene->m_Registry.get<TransformComponent, TextComponent>(ee);
            Renderer2D::DrawString(text.Text, transform.GetTransform(), text.Font, text.Color);
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
        auto view = scene->m_Registry.view<TransformComponent, CameraComponent>();
        for (auto entity : view)
        {
            auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);
            mainCamera = &camera.Camera;
            cameraTransform = transform.GetTransform();
            break;
        }

        // Render the scene
        if (mainCamera)
        {
            /*
            ForwardRenderer::Begin();
            ForwardRenderer::BeginScene(*mainCamera, glm::inverse(cameraTransform));
            auto objs = scene->m_Registry.group<MeshRendererComponent>(entt::get<TransformComponent>);
            for (auto obj : objs)
            {
                auto [transform, mesh] = scene->m_Registry.get<TransformComponent, MeshRendererComponent>(obj);
                //if (mesh.Model)
                //{
                //    ForwardRenderer::Submit(mesh.Model, transform.GetTransform());
                //    // TODO: Update stats... triangle count has to take into account the draw mode
                //}
            }
            ForwardRenderer::Flush();
            ForwardRenderer::EndScene();
            ForwardRenderer::End();*/

            Renderer2D::Begin(*mainCamera, glm::inverse(cameraTransform));
            auto group = scene->m_Registry.group<SpriteRendererComponent>(entt::get<TransformComponent>);
            for (auto ee : group)
            {
                auto [transform, sprite] = scene->m_Registry.get<TransformComponent, SpriteRendererComponent>(ee);
                Renderer2D::FillRect(transform.GetTransform(), sprite.Texture, sprite.Color, (uint32_t)ee);
                s_Stats.Vertices += 4;
                s_Stats.Triangles += 2;
            }
            auto texts = scene->m_Registry.group<TextComponent>(entt::get<TransformComponent>);
            for (auto ee : texts)
            {
                auto [transform, text] = scene->m_Registry.get<TransformComponent, TextComponent>(ee);
                Renderer2D::DrawString(text.Text, transform.GetTransform(), text.Font, text.Color);
                s_Stats.Vertices += (uint32_t)text.Text.size() * 4;
                s_Stats.Triangles += (uint32_t)text.Text.size() * 2;
            }
            Renderer2D::End();
        }
    }

    void SceneRenderer::SetViewportSize(float width, float height)
    {
        s_Data.ViewportWidth = (uint32_t)width;
        s_Data.ViewportHeight = (uint32_t)height;
    }

    void SceneRenderer::Shutdown()
    {
		s_Data.PipelineQuery = nullptr;
        s_Data.Timer2DGeometry = nullptr;
        s_Data.Timer3DGeometry = nullptr;
    }

} // namespace Crowny

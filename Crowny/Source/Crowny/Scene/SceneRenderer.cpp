#include "cwpch.h"

#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scene/SceneRenderer.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/RenderAPI/RenderCommand.h"
#include "Crowny/Renderer/ForwardRenderer.h"
#include "Crowny/Renderer/Renderer2D.h"

namespace Crowny
{

    struct SceneRendererData
    {
        uint32_t ViewportWidth, ViewportHeight;
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
    { /*
         FramebufferProperties fbprops;
         fbprops.Width = 100;
         fbprops.Height = 100;
         fbprops.Attachments = { FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::Depth };
         fbprops.Samples = 8;
         fbprops.ClearColor = { 0.1f, 0.1f, 0.1f, 1.0f };
         s_Data.MainFramebuffer = Framebuffer::Create(fbprops);*/
    }

    // Ref<Framebuffer> SceneRenderer::GetMainFramebuffer() { return s_Data.MainFramebuffer; }

    void SceneRenderer::OnEditorUpdate(Timestep ts, const EditorCamera& camera)
    {
        Ref<Scene> scene = SceneManager::GetActiveScene(); /*
         if (FramebufferProperties spec = s_Data.MainFramebuffer->GetProperties(); s_Data.ViewportWidth > 0.0f
             && s_Data.ViewportHeight > 0.0f && (spec.Width != s_Data.ViewportWidth || spec.Height !=
     s_Data.ViewportHeight))
         {
             //IDBufferRenderer::OnResize(s_Data.ViewportWidth, s_Data.ViewportHeight);
             //s_Data.MainFramebuffer->Resize(s_Data.ViewportWidth, s_Data.ViewportHeight);
             scene->OnViewportResize(s_Data.ViewportWidth, s_Data.ViewportHeight);
         }*/

        /*
        RenderCommand::Clear();
*/

        ForwardRenderer::Begin();
        ForwardRenderer::BeginScene(camera, camera.GetViewMatrix());
        auto objs = scene->m_Registry.group<MeshRendererComponent>(entt::get<TransformComponent>);
        for (auto obj : objs)
        {
            auto [transform, mesh] = scene->m_Registry.get<TransformComponent, MeshRendererComponent>(obj);
            if (mesh.Model)
            {
                ForwardRenderer::Submit(mesh.Model, transform.GetTransform());
                // TODO: Update stats... triangle count has to take into account the draw mode
            }
        }
        ForwardRenderer::Flush();
        ForwardRenderer::EndScene();
        ForwardRenderer::End();

        Renderer2D::Begin(camera, camera.GetViewMatrix());
        auto group = scene->m_Registry.group<SpriteRendererComponent>(entt::get<TransformComponent>);
        for (auto ee : group)
        {
            auto [transform, sprite] = scene->m_Registry.get<TransformComponent, SpriteRendererComponent>(ee);
            Renderer2D::FillRect(transform.GetTransform(), sprite.Texture, sprite.Color, (uint32_t)ee);
            s_Stats.Vertices += 6;
            s_Stats.Triangles += 2;
        }
        auto texts = scene->m_Registry.group<TextComponent>(entt::get<TransformComponent>);
        for (auto ee : texts)
        {
            auto [transform, text] = scene->m_Registry.get<TransformComponent, TextComponent>(ee);
            Renderer2D::DrawString(text.Text, transform.GetTransform(), text.Font, text.Color);
            s_Stats.Vertices += text.Text.size() * 6;
            s_Stats.Triangles += text.Text.size() * 2;
        }
        Renderer2D::End();
        /*
                 IDBufferRenderer::Begin(camera.GetProjection(), camera.GetViewMatrix());
                 {
                     auto group = scene->m_Registry.group<SpriteRendererComponent>(entt::get<TransformComponent>);
                     for (auto entity : group)
                     {
                         auto [transform, sprite] = scene->m_Registry.get<TransformComponent,
           SpriteRendererComponent>(entity); IDBufferRenderer::DrawQuad(transform.GetTransform(), (uint32_t)entity);
                     }
                 }
                 {
                     auto group = scene->m_Registry.group<MeshRendererComponent>(entt::get<TransformComponent>);
                     for (auto entity : group)
                     {
                         auto [transform, mesh] = scene->m_Registry.get<TransformComponent,
           MeshRendererComponent>(entity); if (mesh.Mesh)
                         {
                             IDBufferRenderer::DrawMesh(mesh.Mesh, transform.GetTransform(), (uint32_t)entity);
                         }
                     }
                 }
                 IDBufferRenderer::End();*/
        s_Stats.Frames += 1;
        s_Stats.FrameTime = ts;
    }

    void SceneRenderer::OnRuntimeUpdate(Timestep ts, const Camera& camera, const glm::mat4& cameraTransform) {}

    void SceneRenderer::SetViewportSize(float width, float height)
    {
        s_Data.ViewportWidth = (uint32_t)width;
        s_Data.ViewportHeight = (uint32_t)height;
    }

} // namespace Crowny

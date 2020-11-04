#include "cwpch.h"

#include "Crowny/SceneManagement/Scene.h"
#include "Crowny/SceneManagement/SceneManager.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Renderer/Renderer2D.h"
#include "Crowny/Renderer/RenderCommand.h"
#include "Crowny/Renderer/ForwardRenderer.h"

#include <entt/entt.hpp>

namespace Crowny
{

	Scene::Scene(const std::string& name) : m_Name(name)
	{
		m_RootEntity = new Entity(m_Registry.create(), this);
		m_RootEntity->AddComponent<TagComponent>(m_Name);
		m_RootEntity->AddComponent<RelationshipComponent>();
	}

	Scene::~Scene()
	{
		delete m_RootEntity;
	}

	void Scene::OnUpdate(Timestep ts)
	{
		auto cam = m_Registry.view<CameraComponent>();
		if (!cam.empty()) 
		{
			RenderCommand::SetClearColor(glm::vec4(cam.get<CameraComponent>(cam.front()).Camera.GetBackgroundColor(), 1.0f));
		}

		RenderCommand::Clear();

		m_Registry.group<TransformComponent>(entt::get<CameraComponent>).each([&](const auto& entity, auto& tc, auto& cc)
			{ 
				Renderer2D::Begin(cc.Camera.GetProjectionMatrix(), tc.Transform);

				auto group = m_Registry.group<SpriteRendererComponent>(entt::get<TransformComponent>);
				for (auto ee : group)
				{
					auto [transform, sprite] = m_Registry.get<TransformComponent, SpriteRendererComponent>(ee);
					Renderer2D::FillRect(transform, sprite.Texture, sprite.Color);
				}

				// TODO: Use rect transform, Use Canvas Renderer so I don't have to iterate over each type of ui component
				auto ttt = m_Registry.group<TextComponent>(entt::get<TransformComponent>);
				for (auto ee : ttt)
				{
					auto [transform, tc] = m_Registry.get<TransformComponent, TextComponent>(ee);
					Renderer2D::DrawString(tc.Text, transform, tc.Font, tc.Color);
				}

				Renderer2D::End();

				ForwardRenderer::Begin();
				Camera cam(glm::perspective(glm::radians(45.0f), (float)1280 / (float)720, 0.1f, 1000.0f));
				ForwardRenderer::BeginScene(&cc.Camera, tc.Transform);
				ForwardRenderer::SubmitLightSetup();
				auto objs = m_Registry.group<MeshRendererComponent>(entt::get<TransformComponent>);
				for (auto obj : objs)
				{
					auto [transform, mesh] = m_Registry.get<TransformComponent, MeshRendererComponent>(obj);
					mesh.Mesh->SetMaterialInstnace(CreateRef<MaterialInstance>(ImGuiMaterialPanel::GetSlectedMaterial()));
					ForwardRenderer::SubmitMesh(mesh.Mesh, transform);
				}
				ForwardRenderer::Flush();
				ForwardRenderer::EndScene();
				ForwardRenderer::End();
			});
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		return m_RootEntity->AddChild(name);
	}

	Entity& Scene::GetRootEntity()
	{
		return *m_RootEntity;
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth = width;
		m_ViewportHeight = height;

		m_Registry.view<CameraComponent>().each([&](auto& e, auto& cc) {
		//if(!cc.FixedAspectRatio)
			cc.Camera.SetViewport(width, height);
		});
	}

}
#include "cwpch.h"

#include "Crowny/SceneManagement/Scene.h"
#include "Crowny/SceneManagement/SceneManager.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Renderer/Renderer2D.h"
#include "Crowny/Renderer/RenderCommand.h"
#include "Crowny/Renderer/ForwardRenderer.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptComponent.h"

#include <entt/entt.hpp>

namespace Crowny
{

	Scene::Scene(const std::string& name) : m_Name(name)
	{
		m_RootEntity = new Entity(m_Registry.create(), this);
		m_RootEntity->AddComponent<TagComponent>(m_Name);
		m_RootEntity->AddComponent<RelationshipComponent>();
	}

	Scene::Scene(const Scene& other)
	{
		m_Name = other.m_Name;
		m_RootEntity = other.m_RootEntity;
		//m_Registry = other.m_Registry;
	}

	Scene::~Scene()
	{
		delete m_RootEntity;
	}

	void Scene::Run()
	{
		deltaTime = 0.0f; frameCount = 0.0f; fixedDeltaTime = 0.0f; time = 0.0f; realtimeSinceStarup = 0.0f;

		// Create managed entities
		auto* encl = CWMonoRuntime::GetAssembly("")->GetClass("Crowny", "Entity");
		CW_ENGINE_ASSERT(encl, "Entity class does not exist");
		auto* field1 = encl->GetField("m_InternalPtr");

		m_Registry.each([&](auto entity) {
			MonoObject* enin = encl->CreateInstance();
			uint32_t handle = mono_gchandle_new(enin, false); // TODO: delete this
			ScriptComponent::s_EntityComponents[entity] = enin;
			size_t ent = (size_t)entity;
			field1->Set(enin, &ent);
		});

		// Create managed transforms
		auto* trcl = CWMonoRuntime::GetAssembly("")->GetClass("Crowny", "Transform");
		CW_ENGINE_ASSERT(trcl, "Transform class does not exist");
		auto* field2 = trcl->GetField("m_InternalPtr");

		m_Registry.view<TransformComponent>().each([&](const auto& entity, auto& tc)
		{
			MonoObject* trin = trcl->CreateInstance();
			uint32_t handle = mono_gchandle_new(trin, false); // TODO: delete this!
			tc.ManagedInstance = trin;
			size_t val = (size_t)&tc;
			field2->Set(trin, &val);
		});

		// Create managed scritp components
		m_Registry.view<MonoScriptComponent>().each([&](entt::entity entity, MonoScriptComponent &sc) 
		{
			if (!sc.class)
				return;
				
			auto* field3 = sc.Class->GetField("m_InternalPtr");
			sc.UpdateMethod = sc.Class->GetMethod("Update");
			MonoObject* msin = sc.Class->CreateInstance();
			sc.Instance = msin;
			uint32_t handle = mono_gchandle_new(msin, false); // TODO: delete this
			size_t val = (size_t)&sc;
			field3->Set(msin, &val);

			CWMonoMethod* method = sc.Class->GetMethod("Start", 0);
			if (method)
				method->Call(msin);
		});

		m_Running = true;
	}

	void Scene::OnUpdate(Timestep ts)
	{
		if (!m_Running)
			return;

		deltaTime = ts;
		frameCount += 1;
		time += ts;
		realtimeSinceStarup += ts;

		m_Registry.view<MonoScriptComponent>().each([&](entt::entity entity, MonoScriptComponent &sc) {
			if (sc.UpdateMethod)
				sc.UpdateMethod->Call(sc.Instance);
		});

		auto cam = m_Registry.view<CameraComponent>();
		if (!cam.empty()) 
		{
			RenderCommand::SetClearColor(glm::vec4(cam.get<CameraComponent>(cam.front()).Camera.GetBackgroundColor(), 1.0f));
		}

		RenderCommand::Clear();

		m_Registry.group<TransformComponent>(entt::get<CameraComponent>).each([&](const auto& entity, auto& tc, auto& cc)
			{ 
				Renderer2D::Begin(cc.Camera.GetProjectionMatrix(), tc.GetTransform());

				auto group = m_Registry.group<SpriteRendererComponent>(entt::get<TransformComponent>);
				for (auto ee : group)
				{
					auto [transform, sprite] = m_Registry.get<TransformComponent, SpriteRendererComponent>(ee);
					Renderer2D::FillRect(transform.GetTransform(), sprite.Texture, sprite.Color);
				}

				// TODO: Use rect transform, Use Canvas Renderer so I don't have to iterate over each type of ui component
				auto ttt = m_Registry.group<TextComponent>(entt::get<TransformComponent>);
				for (auto ee : ttt)
				{
					auto [transform, tc] = m_Registry.get<TransformComponent, TextComponent>(ee);
					Renderer2D::DrawString(tc.Text, transform.GetTransform(), tc.Font, tc.Color);
				}

				Renderer2D::End();

				ForwardRenderer::Begin();
				Camera cam(glm::perspective(glm::radians(45.0f), (float)1280 / (float)720, 0.1f, 1000.0f));
				ForwardRenderer::BeginScene(&cc.Camera, tc.GetTransform());
				ForwardRenderer::SubmitLightSetup();
				auto objs = m_Registry.group<MeshRendererComponent>(entt::get<TransformComponent>);
				for (auto obj : objs)
				{
					auto [transform, mesh] = m_Registry.get<TransformComponent, MeshRendererComponent>(obj);
					mesh.Mesh->SetMaterialInstnace(CreateRef<MaterialInstance>(ImGuiMaterialPanel::GetSlectedMaterial()));
					ForwardRenderer::SubmitMesh(mesh.Mesh, transform.GetTransform());
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
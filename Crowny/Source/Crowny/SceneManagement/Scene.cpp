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
		CWMonoAssembly* engine = CWMonoRuntime::GetCrownyAssembly();

		// Create managed entities
		CWMonoClass* entityClass = engine->GetClass("Crowny", "Entity");
		CW_ENGINE_ASSERT(entityClass, "Entity class does not exist");
		CWMonoField* entityPtr = entityClass->GetField("m_InternalPtr");

		m_Registry.each([&](auto entity) {
			MonoObject* entityInstance = entityClass->CreateInstance();
			uint32_t handle = mono_gchandle_new(entityInstance, false); // TODO: delete this
			ScriptComponent::s_EntityComponents[entity] = entityInstance;
			size_t ent = (size_t)entity;
			entityPtr->Set(entityInstance, &ent);
		});

		// Create managed transforms
		CWMonoClass* transformClass = engine->GetClass("Crowny", "Transform");
		CW_ENGINE_ASSERT(transformClass, "Transform class does not exist");
		CWMonoField* transformPtr = transformClass->GetField("m_InternalPtr");

		m_Registry.view<TransformComponent>().each([&](const auto& entity, auto& tc)
		{
			MonoObject* transformInstance = transformClass->CreateInstance();
			uint32_t handle = mono_gchandle_new(transformInstance, false); // TODO: delete this!
			tc.ManagedInstance = transformInstance;
			size_t tmp = (size_t)&tc;
			transformPtr->Set(transformInstance, &tmp);
		});
		
		// Create managed scritp components
		m_Registry.view<MonoScriptComponent>().each([&](entt::entity entity, MonoScriptComponent &sc) 
		{
			if (!sc.Class)
				return;

			CWMonoField* scriptPtr = sc.Class->GetField("m_InternalPtr");
			sc.UpdateMethod = sc.Class->GetMethod("Update");
			MonoObject* scriptInstance = sc.Class->CreateInstance();
			sc.Instance = scriptInstance;
			uint32_t handle = mono_gchandle_new(scriptInstance, false); // TODO: delete this
			size_t tmp = (size_t)&sc;
			scriptPtr->Set(scriptInstance, &tmp);

			CWMonoMethod* start = sc.Class->GetMethod("Start", 0);
			if (start)
				start->Call(scriptInstance);
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
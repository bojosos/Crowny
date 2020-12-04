#include "cwpch.h"

#include "Crowny/Ecs/Entity.h"
#include "Crowny/Scene/Scene.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/Renderer2D.h"
#include "Crowny/Renderer/RenderCommand.h"
#include "Crowny/Renderer/ForwardRenderer.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptComponent.h"

#include <entt/entt.hpp>

namespace Crowny
{

	Scene::Scene(const std::string& name) : m_Name(name), m_HasChanged(true)
	{
		m_RootEntity = new Entity(m_Registry.create(), this);
		
		m_RootEntity->AddComponent<TagComponent>(m_Name);
		m_RootEntity->AddComponent<RelationshipComponent>();
		m_Entities = new std::unordered_map<Uuid, Entity>();
		m_Uuids = new std::unordered_map<Entity, Uuid>();
	}

	Scene::Scene(const Scene& other)
	{
		m_Name = other.m_Name;
		m_RootEntity = other.m_RootEntity;
		//m_Registry = other.m_Registry.clone();
		m_Entities = new std::unordered_map<Uuid, Entity>();
		m_Uuids = new std::unordered_map<Entity, Uuid>();
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
			ScriptComponent::s_EntityComponents[(uint32_t)entity] = entityInstance;
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
		
		// Create managed script components
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

			CWMonoMethod* ctor = sc.Class->GetMethod(".ctor", 0);
			if (ctor)
				ctor->Call(scriptInstance);
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
				const glm::vec4& rect = cc.Camera.GetViewportRect();
				Renderer::SetViewport(rect.x * m_ViewportWidth, rect.y * m_ViewportHeight, rect.z * m_ViewportWidth, rect.w * m_ViewportHeight);
				Renderer2D::Begin(cc.Camera.GetProjection(), tc.GetTransform());

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
		Entity entity = { m_Registry.create(), this };
		Uuid tmp = UuidGenerator::Generate();
		(*m_Entities)[tmp] = entity;
		(*m_Uuids)[entity] = tmp;
		
		entity.AddComponent<TagComponent>(name);
		entity.AddComponent<TransformComponent>();
		entity.AddComponent<RelationshipComponent>(*m_RootEntity);
		m_HasChanged = true;

		return entity;
	}

	Entity Scene::CreateEntity(const Uuid& uuid, const std::string& name)
	{
		Entity entity(m_Registry.create(), this);
		
		(*m_Entities)[uuid] = entity;
		(*m_Uuids)[entity] = uuid;

		entity.AddComponent<TagComponent>(name);
		entity.AddComponent<RelationshipComponent>();
		entity.AddComponent<TransformComponent>();
		m_HasChanged = true;

		return entity;
	}

	Entity Scene::GetEntity(const Uuid& uuid)
	{
		return (*m_Entities)[uuid];
	}

	Uuid& Scene::GetUuid(Entity entity)
	{
		return (*m_Uuids)[entity];
	}

	Entity Scene::GetRootEntity()
	{
		return *m_RootEntity;
	}

	Entity Scene::GetCamera()
	{
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			return Entity{entity, this};
		}
		return {};
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth = width;
		m_ViewportHeight = height;

		m_Registry.view<CameraComponent>().each([&](auto& e, auto& cc) {
		//if(!cc.FixedAspectRatio)
			cc.Camera.SetViewportSize(width, height);
		});
	}

}
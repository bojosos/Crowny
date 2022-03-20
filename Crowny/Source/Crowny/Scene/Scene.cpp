#include "cwpch.h"

#include "Crowny/Scene/Scene.h"

#include "Crowny/Common/Uuid.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/RenderAPI/RenderCommand.h"
#include "Crowny/Renderer/ForwardPlusRenderer.h"
#include "Crowny/Renderer/ForwardRenderer.h"
#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/Renderer2D.h"
#include "Crowny/Scene/SceneManager.h"

#include <entt/entt.hpp>

#include <box2d/box2d.h>

namespace Crowny
{

	template <typename... Component>
	static void CopyComponent(entt::registry& dst, entt::registry& src,
		const UnorderedMap<UUID, entt::entity>& entityMap)
	{
		(
			[&]() {
				auto view = src.view<Component>();
				for (auto srcEntity : view)
				{
					entt::entity dstEntity = entityMap.at(src.get<IDComponent>(srcEntity).Uuid);
					auto& srcComponent = src.get<Component>(srcEntity);
					dst.emplace_or_replace<Component>(dstEntity, srcComponent);
				}
			}(),
				...);
	}

	template <typename... Component>
	static void CopyComponent(ComponentGroup<Component...>, entt::registry& dst, entt::registry& src,
		const UnorderedMap<UUID, entt::entity>& entityMap)
	{
		CopyComponent<Component...>(dst, src, entityMap);
	}

	template <typename... Component> static void CopyComponentIfExists(Entity dst, Entity src)
	{
		(
			[&]() {
				if (src.HasComponent<Component>())
					dst.AddOrReplaceComponent<Component>(src.GetComponent<Component>());
			}(),
				...);
	}

	template <typename... Component>
	static void CopyComponentIfExists(ComponentGroup<Component...>, Entity dst, Entity src)
	{
		CopyComponentIfExists<Component...>(dst, src);
	}

	static void CopyAllComponents(entt::registry& dstRegistry, entt::registry& srcRegistry,
		const UnorderedMap<UUID, entt::entity>& entityMap)
	{
		CopyComponent(AllComponents{}, dstRegistry, srcRegistry, entityMap);
	}

	static void CopyAllExistingComponents(Entity dst, Entity src) { CopyComponentIfExists(AllComponents{}, dst, src); }

	static b2BodyType GetBox2DType(BodyType type)
	{
		switch (type)
		{
		case BodyType::Static:
			return b2_staticBody;
		case BodyType::Dynamic:
			return b2_dynamicBody;
		case BodyType::Kinematic:
			return b2_kinematicBody;
		}
		CW_ENGINE_ASSERT(false);
		return b2_staticBody;
	}

	Scene::Scene(const String& name) : m_Name(name)
	{
		m_RootEntity = new Entity(m_Registry.create(), this);

		m_RootEntity->AddComponent<TransformComponent>();
		m_RootEntity->AddComponent<IDComponent>(UuidGenerator::Generate());
		m_RootEntity->AddComponent<TagComponent>(m_Name);
		m_RootEntity->AddComponent<RelationshipComponent>();
	}

	Scene::Scene(Scene& other)
	{
		m_ViewportWidth = other.m_ViewportWidth;
		m_ViewportHeight = other.m_ViewportHeight;
		m_Filepath = other.m_Filepath;
		m_Name = other.m_Name;

		m_RootEntity = new Entity(m_Registry.create(), this);

		UnorderedMap<UUID, entt::entity> entityMap;

		auto idView = m_Registry.view<IDComponent>();
		for (auto e : idView)
		{
			const UUID& uuid = other.m_Registry.get<IDComponent>(e).Uuid;
			const String& name = other.m_Registry.get<TagComponent>(e).Tag;
			Entity newEntity = CreateEntityWithUuid(uuid, name);
			entityMap[uuid] = e;
		}

		CopyAllComponents(m_Registry, other.m_Registry, entityMap);
	}

	Scene& Scene::operator=(Scene& other)
	{
		if (this == &other)
			return *this;

		m_ViewportWidth = other.m_ViewportWidth;
		m_ViewportHeight = other.m_ViewportHeight;
		m_Filepath = other.m_Filepath;
		m_Name = other.m_Name;

		m_RootEntity = new Entity(m_Registry.create(), this);

		UnorderedMap<UUID, entt::entity> entityMap;

		auto idView = m_Registry.view<IDComponent>();
		for (auto e : idView)
		{
			const UUID& uuid = other.m_Registry.get<IDComponent>(e).Uuid;
			const String& name = other.m_Registry.get<TagComponent>(e).Tag;
			Entity newEntity = CreateEntityWithUuid(uuid, name);
			entityMap[uuid] = e;
		}

		CopyAllComponents(m_Registry, other.m_Registry, entityMap);
		return *this;
	}

	Scene::~Scene() { delete m_RootEntity; }

	Entity Scene::DuplicateEntity(Entity entity, bool includeChildren)
	{
		Entity newEntity = CreateEntity(entity.GetName());
		CopyAllExistingComponents(newEntity, entity);

		const auto& children = entity.GetChildren();
		for (auto child : children)
		{
			Entity e = DuplicateEntity(child);
			e.SetParent(newEntity);
		}
		return newEntity;
	}

	Entity Scene::GetPrimaryCameraEntity()
	{
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			const auto& camera = view.get<CameraComponent>(entity);
			return Entity{ entity, this };
		}
		return {};
	}

	void Scene::OnRuntimeStart()
	{
		m_PhysicsWorld2D = new b2World({ 0.0, -9.8f });

		// Create 2D Rigidbodies
		{
			auto view = m_Registry.view<Rigidbody2DComponent>();
			for (auto e : view)
			{
				Entity entity = { e, this };
				auto& transform = entity.GetTransform();
				auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

				b2BodyDef bodyDef;
				bodyDef.type = GetBox2DType(rb2d.GetBodyType());
				bodyDef.position.Set(transform.Position.x, transform.Position.y);
				bodyDef.angle = transform.Rotation.z;
				b2Body* body = m_PhysicsWorld2D->CreateBody(&bodyDef);
				body->SetFixedRotation(rb2d.GetConstraints().IsSet(Rigidbody2DConstraintsBits::FreezeRotation));
				rb2d.RuntimeBody = body;
			}
		}

		// Create 2D Box Colliders
		{
			auto view = m_Registry.view<BoxCollider2DComponent>();
			for (auto e : view)
			{
				Entity entity = { e, this };
				auto& transform = entity.GetTransform();
				auto& b2d = entity.GetComponent<BoxCollider2DComponent>();

				b2PolygonShape boxShape;
				boxShape.SetAsBox(b2d.Size.x * transform.Scale.x, b2d.Size.y * transform.Scale.y);

				b2FixtureDef fixtureDef;
				fixtureDef.shape = &boxShape;
				fixtureDef.density = b2d.Material.Density;
				fixtureDef.friction = b2d.Material.Friction;
				fixtureDef.restitution = b2d.Material.Restitution;
				fixtureDef.restitutionThreshold = b2d.Material.RestitutionThreshold;
				if (entity.HasComponent<Rigidbody2DComponent>())
					entity.GetComponent<Rigidbody2DComponent>().RuntimeBody->CreateFixture(&fixtureDef);
			}
		}

		// Create 2D Circle Colliders
		{
			auto view = m_Registry.view<CircleCollider2DComponent>();
			for (auto e : view)
			{
				Entity entity = { e, this };
				auto& transform = entity.GetTransform();
				auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();

				b2CircleShape circleShape;
				circleShape.m_p.Set(cc2d.Offset.y, cc2d.Offset.y);
				circleShape.m_radius = transform.Scale.x * cc2d.Radius;

				b2FixtureDef fixtureDef;
				fixtureDef.shape = &circleShape;
				fixtureDef.density = cc2d.Material.Density;
				fixtureDef.friction = cc2d.Material.Friction;
				fixtureDef.restitution = cc2d.Material.Restitution;
				fixtureDef.restitutionThreshold = cc2d.Material.RestitutionThreshold;

				if (entity.HasComponent<Rigidbody2DComponent>())
					entity.GetComponent<Rigidbody2DComponent>().RuntimeBody->CreateFixture(&fixtureDef);
			}
		}
	}

	void Scene::OnRuntimePause() {}

	void Scene::OnRuntimeStop()
	{
		delete m_PhysicsWorld2D; // This should clear everything box2d related.
		m_PhysicsWorld2D = nullptr;
	}

	void Scene::OnUpdateEditor(Timestep ts) {}

	void Scene::OnUpdateRuntime(Timestep ts)
	{
		// update physics
		{
			auto view1 = m_Registry.view<Rigidbody2DComponent>();
			for (auto e : view1)
			{
				Entity entity = { e, this };
				auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
				auto& transform = entity.GetComponent<TransformComponent>();
				if (rb2d.RuntimeBody == nullptr)
				{
					b2BodyDef bodyDef;
					bodyDef.type = GetBox2DType(rb2d.GetBodyType());
					bodyDef.position.Set(transform.Position.x, transform.Position.y);
					bodyDef.angle = transform.Rotation.z;
					b2Body* body = m_PhysicsWorld2D->CreateBody(&bodyDef);
					body->SetFixedRotation(rb2d.GetConstraints().IsSet(Rigidbody2DConstraintsBits::FreezeRotation));
					rb2d.RuntimeBody = body;
				}
				Rigidbody2DConstraints constraints = rb2d.GetConstraints();
				b2Vec2 linVelocty = rb2d.RuntimeBody->GetLinearVelocity();
				if (constraints.IsSet(Rigidbody2DConstraintsBits::FreezePositionX))
					linVelocty.x = 0;
				else if (constraints.IsSet(Rigidbody2DConstraintsBits::FreezePositionY))
					linVelocty.y = 0;
				rb2d.RuntimeBody->SetLinearVelocity(linVelocty);
			}
			const int32_t velocityIterations = 6;
			const int32_t positionIterations = 3; // unity defaults
			m_PhysicsWorld2D->Step(ts, velocityIterations, positionIterations);

			auto view2 = m_Registry.view<Rigidbody2DComponent>();
			for (auto e : view2)
			{
				Entity entity = { e, this };
				auto& transform = entity.GetTransform();
				auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
				b2Body* body = (b2Body*)rb2d.RuntimeBody;
				const auto& position = body->GetPosition();
				transform.Position.x = position.x;
				transform.Position.y = position.y;
				transform.Rotation.z = body->GetAngle();
			}
		}
	}

	Entity Scene::CreateEntity(const String& name)
	{
		Entity entity = { m_Registry.create(), this };
		entity.AddComponent<IDComponent>(UuidGenerator::Generate());
		entity.AddComponent<TagComponent>(name);
		entity.AddComponent<TransformComponent>();
		entity.AddComponent<RelationshipComponent>(*m_RootEntity);

		return entity;
	}

	Entity Scene::CreateEntityWithUuid(const UUID& uuid, const String& name)
	{
		Entity entity(m_Registry.create(), this);

		entity.AddComponent<IDComponent>(uuid);
		entity.AddComponent<TagComponent>(name);
		entity.AddComponent<RelationshipComponent>();
		entity.AddComponent<TransformComponent>();

		return entity;
	}

	Entity Scene::GetEntityFromUuid(const UUID& uuid)
	{
		Entity result;
		m_Registry.each([&](auto entityID) {
			Entity e = { entityID, this };
			if (e.GetUuid() == uuid)
				result = e;
			});

		if (!result)
			CW_ENGINE_ERROR("Entity with uuid {0} not found.", uuid);
		return result;
	}

	Entity Scene::GetRootEntity() { return *m_RootEntity; }

	Entity Scene::FindEntityByName(const String& name)
	{
		auto view = m_Registry.view<TagComponent>();
		for (auto entity : view)
		{
			if (view.get<TagComponent>(entity).Tag == name)
				return Entity(entity, this);
		}
		return Entity{};
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth = width;
		m_ViewportHeight = height;
		m_Registry.view<CameraComponent>().each([&](auto& e, auto& cc) { cc.Camera.SetViewportSize(width, height); });
	}

} // namespace Crowny

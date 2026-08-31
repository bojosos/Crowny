#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Physics/Physics2DBackend.h"
#include "Crowny/Physics/PhysicsCollision.h"
#include "Crowny/Scene/Scene.h"
#include "Crowny/Scene/ScriptRuntime.h"

#include <box2d/box2d.h>

#include <algorithm>
#include <cmath>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <unordered_map>
#include <unordered_set>

namespace Crowny
{
    namespace
    {
        struct WorldTransformComponents
        {
            glm::vec3 Position{ 0.0f };
            glm::vec3 Rotation{ 0.0f };
            glm::vec3 Scale{ 1.0f };
        };

        WorldTransformComponents GetWorldTransformComponents(Entity entity)
        {
            const Transform& world = entity.GetWorldTransform();
            WorldTransformComponents result;
            result.Position = world.GetPosition();
            result.Rotation = glm::eulerAngles(world.GetRotation());
            result.Scale = world.GetScale();
            return result;
        }

        WorldTransformComponents GetWorldTransformComponents(const TransformComponent& transform, Entity parent)
        {
            const Transform& world = transform.GetWorldTransform(parent);
            WorldTransformComponents result;
            result.Position = world.GetPosition();
            result.Rotation = glm::eulerAngles(world.GetRotation());
            result.Scale = world.GetScale();
            return result;
        }

        void SetWorldPose(Entity entity, const b2Vec2& position, float rotation)
        {
            WorldTransformComponents world = GetWorldTransformComponents(entity);
            world.Position.x = position.x;
            world.Position.y = position.y;
            world.Rotation.z = rotation;
            const glm::mat4 matrix =
              glm::translate(glm::mat4(1.0f), world.Position) * glm::toMat4(glm::quat(world.Rotation)) * glm::scale(glm::mat4(1.0f), world.Scale);
            entity.SetWorldTransform(matrix, false);
        }

        b2Body* GetBody(Entity entity)
        {
            if (!entity || !entity.HasComponent<Rigidbody2DComponent>())
                return nullptr;
            return static_cast<b2Body*>(entity.GetComponent<Rigidbody2DComponent>().RuntimeBody);
        }

        b2Body* GetBody(Rigidbody2DComponent& rigidbody) { return static_cast<b2Body*>(rigidbody.RuntimeBody); }

        b2Fixture* GetFixture(Collider2D& collider) { return static_cast<b2Fixture*>(collider.RuntimeFixture); }

        b2BodyType ToBox2DType(RigidbodyBodyType type)
        {
            switch (type)
            {
            case RigidbodyBodyType::Static:
                return b2_staticBody;
            case RigidbodyBodyType::Dynamic:
                return b2_dynamicBody;
            case RigidbodyBodyType::Kinematic:
                return b2_kinematicBody;
            default:
                return b2_staticBody;
            }
        }

        struct ContactSnapshot
        {
            Entity First;
            Entity Second;
            uint64_t ShapeA = 0;
            uint64_t ShapeB = 0;
            Collision2D Collision;
            bool IsTrigger = false;
        };

        enum class ContactEventType
        {
            Enter,
            Stay,
            Exit
        };

        struct ContactEvent
        {
            ContactEventType Type;
            ContactSnapshot Contact;
            uint64_t Sequence = 0;
        };

        class Box2DContactListener final : public b2ContactListener
        {
        public:
            explicit Box2DContactListener(Scene* scene) : m_Scene(scene) {}

            void BeginContact(b2Contact* contact) override
            {
                ContactSnapshot snapshot = Snapshot(contact);
                m_Contacts[contact] = snapshot;
                m_Events.push_back({ ContactEventType::Enter, std::move(snapshot), m_NextSequence++ });
            }

            void EndContact(b2Contact* contact) override
            {
                auto iter = m_Contacts.find(contact);
                ContactSnapshot snapshot = iter != m_Contacts.end() ? iter->second : Snapshot(contact);
                m_Events.push_back({ ContactEventType::Exit, std::move(snapshot), m_NextSequence++ });
                if (iter != m_Contacts.end())
                    m_Contacts.erase(iter);
            }

            void PreSolve(b2Contact* contact, const b2Manifold*) override
            {
                const PhysicsMaterialData firstMaterial = GetMaterial(contact->GetFixtureA());
                const PhysicsMaterialData secondMaterial = GetMaterial(contact->GetFixtureB());
                contact->SetFriction(CombinePhysicsMaterialValue(firstMaterial.Friction, firstMaterial.FrictionCombine,
                                                                 secondMaterial.Friction, secondMaterial.FrictionCombine));
                contact->SetRestitution(CombinePhysicsMaterialValue(firstMaterial.Restitution, firstMaterial.RestitutionCombine,
                                                                    secondMaterial.Restitution, secondMaterial.RestitutionCombine));
                contact->SetRestitutionThreshold(std::min(firstMaterial.RestitutionThreshold, secondMaterial.RestitutionThreshold));

                ContactSnapshot snapshot = Snapshot(contact);
                m_Contacts[contact] = snapshot;
                if (!snapshot.IsTrigger)
                    m_Events.push_back({ ContactEventType::Stay, std::move(snapshot), m_NextSequence++ });
            }

            void QueueTriggerStayEvents()
            {
                for (const auto& entry : m_Contacts)
                {
                    if (entry.second.IsTrigger)
                        m_Events.push_back({ ContactEventType::Stay, entry.second, m_NextSequence++ });
                }
            }

            void Dispatch()
            {
                m_DispatchEvents.clear();
                m_DispatchEvents.swap(m_Events);
                NormalizeEvents(m_DispatchEvents, m_Events);
                for (const ContactEvent& event : m_Events)
                    Dispatch(event);
                m_DispatchEvents.clear();
                m_Events.clear();
            }

            void Clear()
            {
                m_Contacts.clear();
                m_Events.clear();
                m_DispatchEvents.clear();
                m_NextSequence = 0;
            }

        private:
            PhysicsMaterialData GetMaterial(const b2Fixture* fixture) const
            {
                static const PhysicsMaterialData fallback;
                if (!fixture || !m_Scene)
                    return fallback;
                b2Body* body = const_cast<b2Body*>(fixture->GetBody());
                Entity entity(static_cast<entt::entity>(body->GetUserData().pointer), m_Scene);
                if (!entity)
                    return fallback;
                if (entity.HasComponent<BoxCollider2DComponent>())
                {
                    const auto& collider = entity.GetComponent<BoxCollider2DComponent>();
                    if (collider.RuntimeFixture == fixture)
                        return collider.GetMaterialData();
                }
                if (entity.HasComponent<CircleCollider2DComponent>())
                {
                    const auto& collider = entity.GetComponent<CircleCollider2DComponent>();
                    if (collider.RuntimeFixture == fixture)
                        return collider.GetMaterialData();
                }
                return fallback;
            }

            ContactSnapshot Snapshot(b2Contact* contact) const
            {
                b2Fixture* fixtureA = contact->GetFixtureA();
                b2Fixture* fixtureB = contact->GetFixtureB();
                ContactSnapshot result;
                result.First = Entity(static_cast<entt::entity>(fixtureA->GetBody()->GetUserData().pointer), m_Scene);
                result.Second = Entity(static_cast<entt::entity>(fixtureB->GetBody()->GetUserData().pointer), m_Scene);
                result.ShapeA = fixtureA->GetUserData().pointer;
                result.ShapeB = fixtureB->GetUserData().pointer;
                result.IsTrigger = fixtureA->IsSensor() || fixtureB->IsSensor();
                result.Collision.Colliders = { result.First, result.Second };

                if (!result.IsTrigger)
                {
                    b2WorldManifold worldManifold;
                    contact->GetWorldManifold(&worldManifold);
                    const int32_t pointCount = contact->GetManifold()->pointCount;
                    for (int32_t i = 0; i < pointCount; i++)
                        result.Collision.Points.emplace_back(worldManifold.points[i].x, worldManifold.points[i].y);
                }
                return result;
            }

            static std::pair<uint64_t, uint64_t> ContactKey(const ContactEvent& event)
            {
                return event.Contact.ShapeA < event.Contact.ShapeB ? std::pair(event.Contact.ShapeA, event.Contact.ShapeB)
                                                                  : std::pair(event.Contact.ShapeB, event.Contact.ShapeA);
            }

            static void NormalizeEvents(Vector<ContactEvent>& input, Vector<ContactEvent>& output)
            {
                std::sort(input.begin(), input.end(), [](const ContactEvent& lhs, const ContactEvent& rhs) {
                    return std::tuple(ContactKey(lhs), lhs.Type, lhs.Sequence) < std::tuple(ContactKey(rhs), rhs.Type, rhs.Sequence);
                });

                output.clear();
                for (size_t read = 0; read < input.size();)
                {
                    const auto key = ContactKey(input[read]);
                    size_t pairEnd = read + 1;
                    while (pairEnd < input.size() && ContactKey(input[pairEnd]) == key)
                        ++pairEnd;

                    size_t enter = pairEnd;
                    size_t stay = pairEnd;
                    size_t exit = pairEnd;
                    for (size_t index = read; index < pairEnd; ++index)
                    {
                        switch (input[index].Type)
                        {
                        case ContactEventType::Enter:
                            enter = index;
                            break;
                        case ContactEventType::Stay:
                            stay = index;
                            break;
                        case ContactEventType::Exit:
                            exit = index;
                            break;
                        }
                    }

                    if (enter != pairEnd)
                    {
                        if (stay != pairEnd && !input[stay].Contact.Collision.Points.empty())
                            input[enter].Contact.Collision.Points = input[stay].Contact.Collision.Points;
                        output.push_back(std::move(input[enter]));
                    }
                    else if (stay != pairEnd && exit == pairEnd)
                        output.push_back(std::move(input[stay]));
                    if (exit != pairEnd)
                        output.push_back(std::move(input[exit]));

                    read = pairEnd;
                }
            }

            static void DispatchToEntity(const ContactEvent& event, Entity receiver, Entity other, bool reverse)
            {
                if (!receiver || !other || !receiver.HasComponent<ManagedScriptComponent>())
                    return;

                auto& scripts = receiver.GetComponent<ManagedScriptComponent>().Scripts;
                ScriptEvent scriptEvent;
                scriptEvent.OtherEntity = other.GetUuid();
                if (event.Contact.IsTrigger)
                {
                    if (event.Type == ContactEventType::Enter)
                        scriptEvent.Kind = ScriptEventKind::TriggerEnter2D;
                    else if (event.Type == ContactEventType::Stay)
                        scriptEvent.Kind = ScriptEventKind::TriggerStay2D;
                    else
                        scriptEvent.Kind = ScriptEventKind::TriggerExit2D;
                    for (auto& script : scripts)
                        ScriptRuntime::Dispatch(script, scriptEvent);
                    return;
                }

                Collision2D collision = event.Contact.Collision;
                if (reverse)
                    std::swap(collision.Colliders[0], collision.Colliders[1]);
                if (event.Type == ContactEventType::Enter)
                    scriptEvent.Kind = ScriptEventKind::CollisionEnter2D;
                else if (event.Type == ContactEventType::Stay)
                    scriptEvent.Kind = ScriptEventKind::CollisionStay2D;
                else
                    scriptEvent.Kind = ScriptEventKind::CollisionExit2D;
                scriptEvent.Contacts.reserve(collision.Points.size());
                for (const glm::vec2& point : collision.Points)
                    scriptEvent.Contacts.push_back({ glm::vec3(point, 0.0f) });
                for (auto& script : scripts)
                    ScriptRuntime::Dispatch(script, scriptEvent);
            }

            static void Dispatch(const ContactEvent& event)
            {
                DispatchToEntity(event, event.Contact.First, event.Contact.Second, false);
                DispatchToEntity(event, event.Contact.Second, event.Contact.First, true);
            }

            Scene* m_Scene;
            UnorderedMap<b2Contact*, ContactSnapshot> m_Contacts;
            Vector<ContactEvent> m_Events;
            Vector<ContactEvent> m_DispatchEvents;
            uint64_t m_NextSequence = 0;
        };

        class Box2DPhysicsBackend final : public Physics2DBackend
        {
        public:
            ~Box2DPhysicsBackend() override
            {
                delete m_World;
                delete m_ContactListener;
            }

            Physics2DBackendType GetType() const override { return Physics2DBackendType::Box2D; }
            const char* GetName() const override { return "Box2D"; }

            void BeginSimulation(Scene* scene, const Physics2DSettings& settings) override
            {
                if (m_World)
                    StopSimulation(m_Scene);

                m_Scene = scene;
                m_Settings = &settings;
                m_TimestepAccumulator = 0.0f;
                m_World = new b2World({ settings.Gravity.x, settings.Gravity.y });
                m_ContactListener = new Box2DContactListener(scene);
                m_World->SetContactListener(m_ContactListener);

                for (auto handle : scene->GetAllEntitiesWith<Rigidbody2DComponent>())
                    CreateRigidbody(Entity(handle, scene));
                for (auto handle : scene->GetAllEntitiesWith<BoxCollider2DComponent>())
                    CreateBoxCollider(Entity(handle, scene));
                for (auto handle : scene->GetAllEntitiesWith<CircleCollider2DComponent>())
                    CreateCircleCollider(Entity(handle, scene));
            }

            void StopSimulation(Scene* scene) override
            {
                if (!m_World)
                    return;
                if (scene)
                {
                    scene->GetAllEntitiesWith<Rigidbody2DComponent>().each([](Rigidbody2DComponent& rigidbody) {
                        rigidbody.RuntimeBody = nullptr;
                        rigidbody.RuntimeHasPreviousState = false;
                    });
                    scene->GetAllEntitiesWith<BoxCollider2DComponent>().each(
                      [](BoxCollider2DComponent& collider) { collider.RuntimeFixture = nullptr; });
                    scene->GetAllEntitiesWith<CircleCollider2DComponent>().each(
                      [](CircleCollider2DComponent& collider) { collider.RuntimeFixture = nullptr; });
                }
                m_ContactListener->Clear();
                delete m_World;
                delete m_ContactListener;
                m_World = nullptr;
                m_ContactListener = nullptr;
                m_Scene = nullptr;
                m_Settings = nullptr;
                m_TimestepAccumulator = 0.0f;
            }

            bool IsSimulating() const override { return m_World != nullptr; }

            void Step(Timestep timestep, Scene* scene, const Physics2DSettings& settings) override
            {
                if (!m_World || !scene)
                    return;

                const float fixedTimestep = Application::TryGet()->GetTimeSettings()->FixedTimestep;
                if (fixedTimestep <= 0.0f)
                    return;
                const float maxTimestep = std::max(Application::TryGet()->GetTimeSettings()->MaxTimestep, fixedTimestep);
                m_TimestepAccumulator += std::min(static_cast<float>(timestep), maxTimestep);
                auto rigidbodyView = scene->GetAllEntitiesWith<Rigidbody2DComponent, TransformComponent, RelationshipComponent>();

                while (m_TimestepAccumulator >= fixedTimestep)
                {
                    rigidbodyView.each([&](Rigidbody2DComponent& rigidbody, TransformComponent& transform, RelationshipComponent& relationship) {
                        b2Body* body = GetBody(rigidbody);
                        if (!body)
                            return;

                        const b2Vec2 previousPosition = body->GetPosition();
                        rigidbody.RuntimePreviousPosition = { previousPosition.x, previousPosition.y };
                        rigidbody.RuntimePreviousRotation = body->GetAngle();
                        rigidbody.RuntimeHasPreviousState = true;
                        if (rigidbody.GetBodyType() != RigidbodyBodyType::Dynamic)
                        {
                            const WorldTransformComponents world = GetWorldTransformComponents(transform, relationship.Parent);
                            body->SetTransform({ world.Position.x, world.Position.y }, world.Rotation.z);
                        }
                    });

                    m_World->Step(fixedTimestep, static_cast<int32_t>(settings.VelocityIterations),
                                  static_cast<int32_t>(settings.PositionIterations));
                    EnforcePositionConstraints(scene);
                    m_ContactListener->QueueTriggerStayEvents();
                    m_ContactListener->Dispatch();
                    m_TimestepAccumulator -= fixedTimestep;
                }

                const float alpha = std::clamp(m_TimestepAccumulator / fixedTimestep, 0.0f, 1.0f);
                SynchronizeTransforms(scene, alpha);
            }

            void SetGravity(const glm::vec2& gravity) override
            {
                if (m_World)
                    m_World->SetGravity({ gravity.x, gravity.y });
            }

            void CreateRigidbody(Entity entity) override
            {
                if (!m_World || !entity.HasComponent<Rigidbody2DComponent>())
                    return;
                auto& rigidbody = entity.GetComponent<Rigidbody2DComponent>();
                if (rigidbody.RuntimeBody)
                    return;

                const WorldTransformComponents transform = GetWorldTransformComponents(entity);
                b2BodyDef definition;
                definition.position.Set(transform.Position.x, transform.Position.y);
                definition.angle = transform.Rotation.z;
                definition.type = ToBox2DType(rigidbody.GetBodyType());
                definition.allowSleep = rigidbody.GetSleepMode() != RigidbodySleepMode::NeverSleep;
                definition.awake = rigidbody.GetSleepMode() != RigidbodySleepMode::StartSleeping;
                definition.fixedRotation = rigidbody.GetConstraints().IsSet(Rigidbody2DConstraintsBits::FreezeRotation);
                definition.userData.pointer = static_cast<uintptr_t>(entity.GetHandle());
                definition.bullet = rigidbody.GetCollisionDetectionMode() == CollisionDetectionMode2D::Continuous;
                definition.linearDamping = rigidbody.GetLinearDrag();
                definition.angularDamping = rigidbody.GetAngularDrag();
                definition.gravityScale = rigidbody.GetGravityScale();

                b2Body* body = m_World->CreateBody(&definition);
                rigidbody.RuntimeBody = body;
                rigidbody.RuntimePreviousPosition = { body->GetPosition().x, body->GetPosition().y };
                rigidbody.RuntimePreviousRotation = body->GetAngle();
                rigidbody.RuntimeHasPreviousState = true;
            }

            void CreateBoxCollider(Entity entity) override
            {
                if (!m_World || !entity.HasComponent<Rigidbody2DComponent>() || !entity.HasComponent<BoxCollider2DComponent>())
                    return;
                auto& collider = entity.GetComponent<BoxCollider2DComponent>();
                b2Body* body = GetBody(entity);
                if (!body)
                    return;
                if (collider.RuntimeFixture)
                    DestroyFixture(entity, collider);

                const glm::vec3 scale = GetWorldTransformComponents(entity).Scale;
                b2PolygonShape shape;
                shape.SetAsBox(std::abs(collider.GetSize().x * scale.x), std::abs(collider.GetSize().y * scale.y),
                               { collider.GetOffset().x, collider.GetOffset().y }, 0.0f);
                b2FixtureDef definition;
                definition.shape = &shape;
                ApplyFixtureProperties(entity, collider, definition);
                collider.RuntimeFixture = body->CreateFixture(&definition);
                ApplyConfiguredMass(entity);
            }

            void CreateCircleCollider(Entity entity) override
            {
                if (!m_World || !entity.HasComponent<Rigidbody2DComponent>() || !entity.HasComponent<CircleCollider2DComponent>())
                    return;
                auto& collider = entity.GetComponent<CircleCollider2DComponent>();
                b2Body* body = GetBody(entity);
                if (!body)
                    return;
                if (collider.RuntimeFixture)
                    DestroyFixture(entity, collider);

                const glm::vec3 scale = GetWorldTransformComponents(entity).Scale;
                b2CircleShape shape;
                shape.m_p.Set(collider.GetOffset().x, collider.GetOffset().y);
                shape.m_radius = std::abs(collider.GetRadius()) * 0.5f * (std::abs(scale.x) + std::abs(scale.y));
                b2FixtureDef definition;
                definition.shape = &shape;
                ApplyFixtureProperties(entity, collider, definition);
                collider.RuntimeFixture = body->CreateFixture(&definition);
                ApplyConfiguredMass(entity);
            }

            void DestroyRigidbody(Entity entity) override
            {
                b2Body* body = GetBody(entity);
                if (!m_World || !body)
                    return;
                if (entity.HasComponent<BoxCollider2DComponent>())
                    entity.GetComponent<BoxCollider2DComponent>().RuntimeFixture = nullptr;
                if (entity.HasComponent<CircleCollider2DComponent>())
                    entity.GetComponent<CircleCollider2DComponent>().RuntimeFixture = nullptr;
                m_World->DestroyBody(body);
                auto& rigidbody = entity.GetComponent<Rigidbody2DComponent>();
                rigidbody.RuntimeBody = nullptr;
                rigidbody.RuntimeHasPreviousState = false;
            }

            void DestroyFixture(Entity entity, Collider2D& collider) override
            {
                b2Body* body = GetBody(entity);
                b2Fixture* fixture = GetFixture(collider);
                if (!body || !fixture)
                    return;
                body->DestroyFixture(fixture);
                collider.RuntimeFixture = nullptr;
            }

            bool IsBodyAwake(Entity entity) const override
            {
                const b2Body* body = GetBody(entity);
                return body && body->IsAwake();
            }

            float GetMass(Entity entity) const override
            {
                const b2Body* body = GetBody(entity);
                return body ? body->GetMass() : 0.0f;
            }

            void SetTransform(Entity entity) override
            {
                b2Body* body = GetBody(entity);
                if (!body)
                    return;
                const WorldTransformComponents transform = GetWorldTransformComponents(entity);
                const b2Vec2 position(transform.Position.x, transform.Position.y);
                const float rotation = transform.Rotation.z;
                const b2Vec2 currentPosition = body->GetPosition();
                if (b2DistanceSquared(position, currentPosition) <= b2_epsilon * b2_epsilon && std::abs(rotation - body->GetAngle()) <= b2_epsilon)
                    return;
                body->SetTransform(position, rotation);
                auto& rigidbody = entity.GetComponent<Rigidbody2DComponent>();
                rigidbody.RuntimePreviousPosition = { position.x, position.y };
                rigidbody.RuntimePreviousRotation = rotation;
                rigidbody.RuntimeHasPreviousState = true;
            }

            float GetInertia(Entity entity) const override
            {
                const b2Body* body = GetBody(entity);
                return body ? body->GetInertia() : 0.0f;
            }

            glm::vec2 GetCenterOfMass(Entity entity) const override
            {
                const b2Body* body = GetBody(entity);
                if (!body)
                    return glm::vec2(0.0f);
                const b2Vec2 center = body->GetLocalCenter();
                return { center.x, center.y };
            }

            glm::vec2 GetPosition(Entity entity) const override
            {
                const b2Body* body = GetBody(entity);
                return body ? glm::vec2(body->GetPosition().x, body->GetPosition().y) : glm::vec2(0.0f);
            }

            float GetRotation(Entity entity) const override
            {
                const b2Body* body = GetBody(entity);
                return body ? body->GetAngle() : 0.0f;
            }

            glm::vec2 GetLinearVelocity(Entity entity) const override
            {
                const b2Body* body = GetBody(entity);
                return body ? glm::vec2(body->GetLinearVelocity().x, body->GetLinearVelocity().y) : glm::vec2(0.0f);
            }

            float GetAngularVelocity(Entity entity) const override
            {
                const b2Body* body = GetBody(entity);
                return body ? body->GetAngularVelocity() : 0.0f;
            }

            void SetLinearVelocity(Entity entity, const glm::vec2& velocity) override
            {
                if (b2Body* body = GetBody(entity))
                    body->SetLinearVelocity({ velocity.x, velocity.y });
            }

            void SetAngularVelocity(Entity entity, float velocity) override
            {
                if (b2Body* body = GetBody(entity))
                    body->SetAngularVelocity(velocity);
            }

            void SetBodyAwake(Entity entity, bool awake) override
            {
                if (b2Body* body = GetBody(entity))
                    body->SetAwake(awake);
            }

            void SetLayer(Rigidbody2DComponent& rigidbody, uint32_t, uint32_t categoryBits, uint32_t maskBits) override
            {
                b2Body* body = GetBody(rigidbody);
                if (!body)
                    return;
                for (b2Fixture* fixture = body->GetFixtureList(); fixture; fixture = fixture->GetNext())
                {
                    b2Filter filter = fixture->GetFilterData();
                    filter.categoryBits = static_cast<uint16_t>(categoryBits);
                    filter.maskBits = static_cast<uint16_t>(maskBits);
                    fixture->SetFilterData(filter);
                    fixture->Refilter();
                }
            }

            void SetBodyType(Rigidbody2DComponent& rigidbody) override
            {
                if (b2Body* body = GetBody(rigidbody))
                    body->SetType(ToBox2DType(rigidbody.GetBodyType()));
            }

            void SetMass(Rigidbody2DComponent& rigidbody, float mass) override
            {
                b2Body* body = GetBody(rigidbody);
                if (!body || body->GetType() != b2_dynamicBody)
                    return;
                b2MassData data;
                body->GetMassData(&data);
                data.mass = std::max(mass, 0.0001f);
                body->SetMassData(&data);
            }

            void SetInertia(Rigidbody2DComponent& rigidbody, float inertia) override
            {
                b2Body* body = GetBody(rigidbody);
                if (!body || body->GetType() != b2_dynamicBody)
                    return;
                b2MassData data;
                body->GetMassData(&data);
                data.I = std::max(inertia, 0.0f);
                body->SetMassData(&data);
            }

            void ResetMass(Entity entity) override
            {
                if (b2Body* body = GetBody(entity))
                    body->ResetMassData();
            }

            void SetGravityScale(Rigidbody2DComponent& rigidbody, float scale) override
            {
                if (b2Body* body = GetBody(rigidbody))
                    body->SetGravityScale(scale);
            }

            void SetConstraints(Rigidbody2DComponent& rigidbody) override
            {
                if (b2Body* body = GetBody(rigidbody))
                    body->SetFixedRotation(rigidbody.GetConstraints().IsSet(Rigidbody2DConstraintsBits::FreezeRotation));
            }

            void SetCollisionDetectionMode(Rigidbody2DComponent& rigidbody) override
            {
                if (b2Body* body = GetBody(rigidbody))
                    body->SetBullet(rigidbody.GetCollisionDetectionMode() == CollisionDetectionMode2D::Continuous);
            }

            void SetSleepMode(Rigidbody2DComponent& rigidbody) override
            {
                b2Body* body = GetBody(rigidbody);
                if (!body)
                    return;
                const RigidbodySleepMode mode = rigidbody.GetSleepMode();
                body->SetSleepingAllowed(mode != RigidbodySleepMode::NeverSleep);
                if (mode == RigidbodySleepMode::StartSleeping)
                    body->SetAwake(false);
                else if (mode == RigidbodySleepMode::StartAwake || mode == RigidbodySleepMode::NeverSleep)
                    body->SetAwake(true);
            }

            void SetLinearDrag(Rigidbody2DComponent& rigidbody, float linearDrag) override
            {
                if (b2Body* body = GetBody(rigidbody))
                    body->SetLinearDamping(std::max(linearDrag, 0.0f));
            }

            void SetAngularDrag(Rigidbody2DComponent& rigidbody, float angularDrag) override
            {
                if (b2Body* body = GetBody(rigidbody))
                    body->SetAngularDamping(std::max(angularDrag, 0.0f));
            }

            void SetCenterOfMass(Rigidbody2DComponent& rigidbody, const glm::vec2& center) override
            {
                b2Body* body = GetBody(rigidbody);
                if (!body || body->GetType() != b2_dynamicBody)
                    return;
                b2MassData data;
                body->GetMassData(&data);
                data.center = { center.x, center.y };
                body->SetMassData(&data);
            }

            void SetTrigger(Collider2D& collider, bool trigger) override
            {
                if (b2Fixture* fixture = GetFixture(collider))
                    fixture->SetSensor(trigger);
            }

            void SetMaterial(Collider2D& collider) override
            {
                b2Fixture* fixture = GetFixture(collider);
                if (!fixture)
                    return;
                const PhysicsMaterialData material = collider.GetMaterialData();
                fixture->SetDensity(material.Density);
                fixture->SetFriction(material.Friction);
                fixture->SetRestitution(material.Restitution);
                fixture->SetRestitutionThreshold(material.RestitutionThreshold);
                fixture->GetBody()->ResetMassData();
                if (m_Scene)
                {
                    Entity entity(static_cast<entt::entity>(fixture->GetBody()->GetUserData().pointer), m_Scene);
                    if (entity)
                        ApplyConfiguredMass(entity);
                }
            }

            void AddForce(Entity entity, const glm::vec2& force, bool impulse) override
            {
                b2Body* body = GetBody(entity);
                if (!body)
                    return;
                if (impulse)
                    body->ApplyLinearImpulseToCenter({ force.x, force.y }, true);
                else
                    body->ApplyForceToCenter({ force.x, force.y }, true);
            }

            void AddForceAt(Entity entity, const glm::vec2& force, const glm::vec2& worldPosition, bool impulse) override
            {
                b2Body* body = GetBody(entity);
                if (!body)
                    return;
                if (impulse)
                    body->ApplyLinearImpulse({ force.x, force.y }, { worldPosition.x, worldPosition.y }, true);
                else
                    body->ApplyForce({ force.x, force.y }, { worldPosition.x, worldPosition.y }, true);
            }

            void AddTorque(Entity entity, float torque, bool impulse) override
            {
                b2Body* body = GetBody(entity);
                if (!body)
                    return;
                if (impulse)
                    body->ApplyAngularImpulse(torque, true);
                else
                    body->ApplyTorque(torque, true);
            }

            Vector<PhysicsRaycastHit2D> Raycast(const glm::vec2& origin, const glm::vec2& direction, float distance,
                                                uint32_t layerMask) const override
            {
                class Callback final : public b2RayCastCallback
                {
                public:
                    Callback(Scene* scene, uint32_t layerMask) : ScenePtr(scene), LayerMask(layerMask) {}

                    float ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float fraction) override
                    {
                        if ((fixture->GetFilterData().categoryBits & LayerMask) == 0)
                            return -1.0f;
                        PhysicsRaycastHit2D hit;
                        hit.Point = { point.x, point.y };
                        hit.Normal = { normal.x, normal.y };
                        hit.Fraction = fraction;
                        hit.HitEntity = Entity(static_cast<entt::entity>(fixture->GetBody()->GetUserData().pointer), ScenePtr);
                        Hits.push_back(hit);
                        return 1.0f;
                    }

                    Scene* ScenePtr;
                    uint32_t LayerMask;
                    Vector<PhysicsRaycastHit2D> Hits;
                } callback(m_Scene, layerMask);

                const glm::vec2 normalizedDirection = glm::normalize(direction);
                const glm::vec2 end = origin + normalizedDirection * distance;
                m_World->RayCast(&callback, { origin.x, origin.y }, { end.x, end.y });
                std::sort(callback.Hits.begin(), callback.Hits.end(), [](const auto& lhs, const auto& rhs) { return lhs.Fraction < rhs.Fraction; });
                return callback.Hits;
            }

            uint32_t GetBodyCount() const override { return m_World ? static_cast<uint32_t>(m_World->GetBodyCount()) : 0; }

        private:
            void ApplyFixtureProperties(Entity entity, const Collider2D& collider, b2FixtureDef& definition) const
            {
                const uint32_t layer = std::min(entity.GetComponent<Rigidbody2DComponent>().GetLayerMask(), Physics2DLayerCount - 1);
                definition.filter.categoryBits = static_cast<uint16_t>(1u << layer);
                definition.filter.maskBits = static_cast<uint16_t>(m_Settings ? m_Settings->MaskBits[layer] : 0xFFFFu);
                definition.userData.pointer = collider.InstanceId;
                definition.isSensor = collider.IsTrigger();
                const PhysicsMaterialData material = collider.GetMaterialData();
                definition.density = material.Density;
                definition.friction = material.Friction;
                definition.restitution = material.Restitution;
                definition.restitutionThreshold = material.RestitutionThreshold;
            }

            static void ApplyConfiguredMass(Entity entity)
            {
                auto& rigidbody = entity.GetComponent<Rigidbody2DComponent>();
                b2Body* body = GetBody(entity);
                if (!body || body->GetType() != b2_dynamicBody)
                    return;
                if (!rigidbody.GetAutoMass())
                {
                    b2MassData data;
                    body->GetMassData(&data);
                    data.mass = std::max(rigidbody.GetConfiguredMass(), 0.0001f);
                    data.I = std::max(rigidbody.GetConfiguredInertia(), 0.0f);
                    data.center = { rigidbody.GetConfiguredCenterOfMass().x, rigidbody.GetConfiguredCenterOfMass().y };
                    body->SetMassData(&data);
                }
            }

            void EnforcePositionConstraints(Scene* scene)
            {
                scene->GetAllEntitiesWith<Rigidbody2DComponent>().each([&](Rigidbody2DComponent& rigidbody) {
                    b2Body* body = GetBody(rigidbody);
                    if (!body || body->GetType() != b2_dynamicBody)
                        return;

                    const auto constraints = rigidbody.GetConstraints();
                    if (!rigidbody.RuntimeHasPreviousState)
                        return;
                    b2Vec2 position = body->GetPosition();
                    b2Vec2 velocity = body->GetLinearVelocity();
                    if (constraints.IsSet(Rigidbody2DConstraintsBits::FreezePositionX))
                    {
                        position.x = rigidbody.RuntimePreviousPosition.x;
                        velocity.x = 0.0f;
                    }
                    if (constraints.IsSet(Rigidbody2DConstraintsBits::FreezePositionY))
                    {
                        position.y = rigidbody.RuntimePreviousPosition.y;
                        velocity.y = 0.0f;
                    }
                    body->SetTransform(position, body->GetAngle());
                    body->SetLinearVelocity(velocity);
                });
            }

            void SynchronizeTransforms(Scene* scene, float alpha)
            {
                scene->GetAllEntitiesWith<Rigidbody2DComponent>().each([&](entt::entity handle, Rigidbody2DComponent& rigidbody) {
                    Entity entity(handle, scene);
                    b2Body* body = GetBody(rigidbody);
                    if (!body || rigidbody.GetBodyType() == RigidbodyBodyType::Static)
                        return;

                    const b2Vec2 currentPosition = body->GetPosition();
                    const float currentRotation = body->GetAngle();
                    b2Vec2 outputPosition = currentPosition;
                    float outputRotation = currentRotation;
                    if (rigidbody.GetInterpolationMode() == RigidbodyInterpolation::Interpolate && rigidbody.RuntimeHasPreviousState)
                    {
                        const b2Vec2 previousPosition{ rigidbody.RuntimePreviousPosition.x, rigidbody.RuntimePreviousPosition.y };
                        outputPosition = (1.0f - alpha) * previousPosition + alpha * currentPosition;
                        outputRotation = (1.0f - alpha) * rigidbody.RuntimePreviousRotation + alpha * currentRotation;
                    }
                    else if (rigidbody.GetInterpolationMode() == RigidbodyInterpolation::Extrapolate)
                    {
                        outputPosition += m_TimestepAccumulator * body->GetLinearVelocity();
                        outputRotation += m_TimestepAccumulator * body->GetAngularVelocity();
                    }

                    SetWorldPose(entity, outputPosition, outputRotation);
                });
            }

            b2World* m_World = nullptr;
            Box2DContactListener* m_ContactListener = nullptr;
            Scene* m_Scene = nullptr;
            const Physics2DSettings* m_Settings = nullptr;
            float m_TimestepAccumulator = 0.0f;
        };
    } // namespace

    Scope<Physics2DBackend> CreateBox2DBackend() { return CreateScope<Box2DPhysicsBackend>(); }
} // namespace Crowny

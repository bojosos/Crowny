#include "cwpch.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Physics/Physics2D.h"

#include <imgui.h>

#include <box2d/box2d.h>

namespace Crowny
{

    static b2BodyType GetBox2DType(RigidbodyBodyType type)
    {
        switch (type)
        {
        case RigidbodyBodyType::Static:
            return b2_staticBody;
        case RigidbodyBodyType::Dynamic:
            return b2_dynamicBody;
        case RigidbodyBodyType::Kinematic:
            return b2_kinematicBody;
        }
        CW_ENGINE_ASSERT(false);
        return b2_staticBody;
    }

    class ContactListener : public b2ContactListener
    {
    public:
        ContactListener(Scene* scene);

        virtual void BeginContact(b2Contact* contact);
        virtual void EndContact(b2Contact* contact);

    private:
        Scene* m_Scene;
    };

    ContactListener::ContactListener(Scene* scene) : m_Scene(scene) {}

    void ContactListener::BeginContact(b2Contact* contact)
    {
        Entity e1 = Entity((entt::entity)contact->GetFixtureA()->GetBody()->GetUserData().pointer, m_Scene);
        Entity e2 = Entity((entt::entity)contact->GetFixtureB()->GetBody()->GetUserData().pointer, m_Scene);

        auto callbacks = [&](Entity e1, Entity e2) {
            if (e1.HasComponent<MonoScriptComponent>())
            {
                auto& monoScript = e1.GetComponent<MonoScriptComponent>();
                bool sendTriggerCallback = contact->GetFixtureA()->IsSensor();
                if (!sendTriggerCallback)
                {
                    b2WorldManifold manifold;
                    contact->GetWorldManifold(&manifold);
                    Collision2D collision;
                    collision.Points.push_back(glm::vec2(manifold.points[0].x, manifold.points[0].y));
                    collision.Points.push_back(glm::vec2(manifold.points[1].x, manifold.points[1].y));
                    collision.Colliders.push_back(e1);
                    collision.Colliders.push_back(e2);

                    for (auto& script : monoScript.Scripts)
                        script.OnCollisionEnter2D(collision);
                }
                else
                {
                    for (auto& script : monoScript.Scripts)
                        script.OnTriggerEnter2D(e2);
                }
            }
        };
        callbacks(e1, e2);
        callbacks(e2, e1);
    }

    void ContactListener::EndContact(b2Contact* contact)
    {
        Entity e1 = Entity((entt::entity)contact->GetFixtureA()->GetBody()->GetUserData().pointer, m_Scene);
        Entity e2 = Entity((entt::entity)contact->GetFixtureB()->GetBody()->GetUserData().pointer, m_Scene);

        auto callbacks = [&](Entity e1, Entity e2, bool sendTriggerCallback) {
            if (e1.HasComponent<MonoScriptComponent>())
            {
                auto& monoScript = e1.GetComponent<MonoScriptComponent>();
                if (!sendTriggerCallback)
                {
                    b2WorldManifold manifold;
                    contact->GetWorldManifold(&manifold);
                    Collision2D collision;
                    collision.Points.push_back(glm::vec2(manifold.points[0].x, manifold.points[0].y));
                    collision.Points.push_back(glm::vec2(manifold.points[1].x, manifold.points[1].y));
                    collision.Colliders.push_back(e1);
                    collision.Colliders.push_back(e2);

                    for (auto& script : monoScript.Scripts)
                        script.OnCollisionExit2D(collision);
                }
                else
                {
                    for (auto& script : monoScript.Scripts)
                        script.OnTriggerExit2D(e2);
                }
            }
        };
        callbacks(e1, e2, contact->GetFixtureB()->IsSensor());
        callbacks(e2, e1, contact->GetFixtureA()->IsSensor());
    }

    Physics2D::Physics2D()
    {
        m_Settings = CreateRef<Physics2DSettings>();
        m_Settings->DefaultMaterial =
          static_asset_cast<PhysicsMaterial2D>(AssetManager::Get().CreateAssetHandle(CreateRef<PhysicsMaterial2D>()));
        for (uint32_t i = 0; i < m_Settings->MaskBits.size(); i++)
            m_Settings->MaskBits[i] = 0xFFFFFFFF;
        m_TemporaryWorld2D = new b2World({ m_Settings->Gravity.x, m_Settings->Gravity.y });
    }

    void Physics2D::UIStats()
    {
        ImGui::Begin("Physics2D Stats");
        if (m_PhysicsWorld2D == nullptr)
        {
            ImGui::End();
            return;
        }
        ImGui::Columns(2);
        ImGui::Text("Body count");
        ImGui::NextColumn();
        ImGui::Text("%d", m_PhysicsWorld2D->GetBodyCount());
        ImGui::NextColumn();
        uint32_t i = 0;
        for (b2Body* body = m_PhysicsWorld2D->GetBodyList(); body; body = body->GetNext())
        {
            ImGui::Text("%d", i);
            ImGui::NextColumn();
            ImGui::Text("%d%f%f", body->GetType(), body->GetPosition().x, body->GetPosition().y);
            ImGui::NextColumn();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20.0f);
            uint32_t idx = 0;
            for (b2Fixture* f = body->GetFixtureList(); f != nullptr; f = f->GetNext())
            {
                ImGui::Text("%d", idx++);
                ImGui::NextColumn();
                ImGui::NextColumn();
            }
        }
        ImGui::Columns(1);
        ImGui::End();
    }

    void Physics2D::SetGravity(const glm::vec2& gravity)
    {
        if (m_PhysicsWorld2D != nullptr)
            m_PhysicsWorld2D->SetGravity({ gravity.x, gravity.y });
        m_Settings->Gravity = gravity;
    }
    void Physics2D::SetVelocityIterations(uint32_t iterations) { m_Settings->VelocityIterations = iterations; }

    void Physics2D::SetPositionIterations(uint32_t iterations) { m_Settings->PositionIterations = iterations; }

    void Physics2D::SetCategoryMask(uint32_t idx, uint32_t mask)
    {
        m_Settings->MaskBits[idx] = mask;
        if (SceneManager::GetSceneCount() == 0)
            return;
        Scene* scene = SceneManager::GetActiveScene().get();
        auto view = scene->GetAllEntitiesWith<Rigidbody2DComponent>();
        for (auto e : view)
        {
            Entity entity{ e, scene };
            auto& rb = entity.GetComponent<Rigidbody2DComponent>();
            if (rb.GetLayerMask() == idx)
                rb.SetLayerMask(idx, entity);
        }
    }

    Physics2D::~Physics2D()
    {
        delete m_TemporaryWorld2D;
        delete m_PhysicsWorld2D;
        delete m_ContactListener2D;
    }

    void Physics2D::BeginSimulation(Scene* scene)
    {
        if (m_ContactListener2D == nullptr)
            m_ContactListener2D = new ContactListener(scene); // Perhaps don't do this every time simulation starts
        m_PhysicsWorld2D = new b2World({ m_Settings->Gravity.x, m_Settings->Gravity.y });
        m_PhysicsWorld2D->SetContactListener(m_ContactListener2D);

        auto bodies = scene->GetAllEntitiesWith<Rigidbody2DComponent>();
        for (auto e : bodies)
        {
            Entity entity = { e, scene };
            CreateRigidbody(entity);
        }

        auto boxColliders = scene->GetAllEntitiesWith<BoxCollider2DComponent>();
        for (auto e : boxColliders)
        {
            Entity entity = { e, scene };
            CreateBoxCollider(entity);
        }

        auto circleColliders = scene->GetAllEntitiesWith<CircleCollider2DComponent>();
        for (auto e : circleColliders)
        {
            Entity entity = { e, scene };
            CreateCircleCollider(entity);
        }
    }

    void Physics2D::CreateRigidbody(Entity entity)
    {
        auto& rigidBody2D = entity.GetComponent<Rigidbody2DComponent>();
        const auto& transform = entity.GetComponent<TransformComponent>();
        b2BodyDef bodyDef;
        bodyDef.position.Set(transform.Position.x, transform.Position.y);
        bodyDef.angle = transform.Rotation.z;

        bodyDef.type = GetBox2DType(rigidBody2D.GetBodyType());
        bodyDef.allowSleep = rigidBody2D.GetSleepMode() != RigidbodySleepMode::NeverSleep;
        bodyDef.awake = rigidBody2D.GetSleepMode() == RigidbodySleepMode::StartAwake ||
                        rigidBody2D.GetSleepMode() == RigidbodySleepMode::NeverSleep;
        bodyDef.fixedRotation = rigidBody2D.GetConstraints().IsSet(Rigidbody2DConstraintsBits::FreezeRotation);
        bodyDef.userData.pointer = (uintptr_t)entity.GetHandle();
        bodyDef.bullet = rigidBody2D.GetCollisionDetectionMode() == CollisionDetectionMode2D::Continuous;
        bodyDef.linearDamping = rigidBody2D.GetGravityScale();
        bodyDef.angularDamping = rigidBody2D.GetAngularDrag();

        b2Body* body = m_PhysicsWorld2D->CreateBody(&bodyDef);
        b2MassData massData;
        massData.mass = rigidBody2D.GetMass();
        body->SetMassData(&massData);
        rigidBody2D.RuntimeBody = body;
    }

    void Physics2D::CreateBoxCollider(Entity entity)
    {
        auto& transform = entity.GetTransform();
        auto& b2d = entity.GetComponent<BoxCollider2DComponent>();

        b2PolygonShape boxShape;
        boxShape.SetAsBox(b2d.GetSize().x * transform.Scale.x, b2d.GetSize().y * transform.Scale.y,
                          { b2d.GetOffset().x, b2d.GetOffset().y }, 0.0f);

        b2FixtureDef fixtureDef;
        fixtureDef.shape = &boxShape;

        uint32_t layerMask =
          entity.HasComponent<Rigidbody2DComponent>() ? entity.GetComponent<Rigidbody2DComponent>().GetLayerMask() : 0;
        fixtureDef.filter.maskBits = 1 << layerMask;
        fixtureDef.filter.categoryBits = Physics2D::Get().GetCategoryMask(layerMask);

        fixtureDef.isSensor = b2d.IsTrigger();

        // TODO: Move to apply material function
        fixtureDef.density = b2d.GetMaterial()->m_Density;
        fixtureDef.friction = b2d.GetMaterial()->m_Friction;
        fixtureDef.restitution = b2d.GetMaterial()->m_Restitution;
        fixtureDef.restitutionThreshold = b2d.GetMaterial()->m_RestitutionThreshold;

        if (entity.HasComponent<Rigidbody2DComponent>())
            b2d.RuntimeFixture = entity.GetComponent<Rigidbody2DComponent>().RuntimeBody->CreateFixture(&fixtureDef);
    }

    void Physics2D::CreateCircleCollider(Entity entity)
    {
        auto& transform = entity.GetTransform();
        auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();

        b2CircleShape circleShape;
        circleShape.m_p.Set(cc2d.GetOffset().x, cc2d.GetOffset().y);
        circleShape.m_radius = (transform.Scale.x + transform.Scale.y) * 0.5f * cc2d.GetRadius();

        b2FixtureDef fixtureDef;
        fixtureDef.shape = &circleShape;

        uint32_t layerMask =
          entity.HasComponent<Rigidbody2DComponent>() ? entity.GetComponent<Rigidbody2DComponent>().GetLayerMask() : 0;
        fixtureDef.filter.maskBits = 1 << layerMask;
        fixtureDef.filter.categoryBits = Physics2D::Get().GetCategoryMask(layerMask);

        fixtureDef.isSensor = cc2d.IsTrigger();

        fixtureDef.density = cc2d.GetMaterial()->m_Density;
        fixtureDef.friction = cc2d.GetMaterial()->m_Friction;
        fixtureDef.restitution = cc2d.GetMaterial()->m_Restitution;
        fixtureDef.restitutionThreshold = cc2d.GetMaterial()->m_RestitutionThreshold;

        if (entity.HasComponent<Rigidbody2DComponent>())
            cc2d.RuntimeFixture = entity.GetComponent<Rigidbody2DComponent>().RuntimeBody->CreateFixture(&fixtureDef);
    }

    void Physics2D::DestroyRigidbody(Entity entity)
    {
        m_PhysicsWorld2D->DestroyBody(entity.GetComponent<Rigidbody2DComponent>().RuntimeBody);
        entity.GetComponent<Rigidbody2DComponent>().RuntimeBody = nullptr;
    }

    void Physics2D::DestroyFixture(Entity entity, Collider2D& collider)
    {
        entity.GetComponent<Rigidbody2DComponent>().RuntimeBody->DestroyFixture(collider.RuntimeFixture);
        collider.RuntimeFixture = nullptr;
    }

    void Physics2D::Step(Timestep ts, Scene* scene)
    {
        auto view1 = scene->GetAllEntitiesWith<Rigidbody2DComponent>();
        for (auto e : view1)
        {
            Entity entity = { e, scene };
            auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
            auto& transform = entity.GetComponent<TransformComponent>();
            Rigidbody2DConstraints constraints = rb2d.GetConstraints();
            b2Vec2 linVelocty = rb2d.RuntimeBody->GetLinearVelocity();
            if (constraints.IsSet(Rigidbody2DConstraintsBits::FreezePositionX))
                linVelocty.x = 0;
            else if (constraints.IsSet(Rigidbody2DConstraintsBits::FreezePositionY))
                linVelocty.y = 0;
            rb2d.RuntimeBody->SetLinearVelocity(linVelocty);
        }

        m_PhysicsWorld2D->Step(ts, m_Settings->VelocityIterations, m_Settings->PositionIterations);

        auto view2 = scene->GetAllEntitiesWith<Rigidbody2DComponent>();
        for (auto e : view2)
        {
            Entity entity = { e, scene };
            auto& transform = entity.GetTransform();
            auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
            b2Body* body = (b2Body*)rb2d.RuntimeBody;
            const auto& position = body->GetPosition();
            transform.Position.x = position.x;
            transform.Position.y = position.y;
            transform.Rotation.z = body->GetAngle();
        }
    }

    void Physics2D::StopSimulation(Scene* scene)
    {
        auto rbView = scene->GetAllEntitiesWith<Rigidbody2DComponent>();
        for (auto e : rbView)
            Entity(e, scene).GetComponent<Rigidbody2DComponent>().RuntimeBody = nullptr;
        auto bcView = scene->GetAllEntitiesWith<BoxCollider2DComponent>();
        for (auto e : bcView)
            Entity(e, scene).GetComponent<BoxCollider2DComponent>().RuntimeFixture = nullptr;
        auto ccView = scene->GetAllEntitiesWith<CircleCollider2DComponent>();
        for (auto e : ccView)
            Entity(e, scene).GetComponent<CircleCollider2DComponent>().RuntimeFixture = nullptr;
        delete m_PhysicsWorld2D; // This should clear everything box2d related.
        m_PhysicsWorld2D = nullptr;
    }

    float Physics2D::CalculateMass(Entity entity)
    {
        if (!entity.HasComponent<Rigidbody2DComponent>())
            return 0.0f;
        if (m_PhysicsWorld2D != nullptr)
            return entity.GetComponent<Rigidbody2DComponent>().RuntimeBody->GetMass();
        b2World* tempWorld = m_PhysicsWorld2D;
        m_PhysicsWorld2D = m_TemporaryWorld2D;

        CreateRigidbody(entity);
        if (entity.HasComponent<BoxCollider2DComponent>())
            CreateBoxCollider(entity);
        if (entity.HasComponent<CircleCollider2DComponent>())
            CreateCircleCollider(entity);
        float mass = entity.GetComponent<Rigidbody2DComponent>().GetMass();
        if (entity.HasComponent<BoxCollider2DComponent>())
            DestroyFixture(entity, entity.GetComponent<BoxCollider2DComponent>());
        if (entity.HasComponent<CircleCollider2DComponent>())
            DestroyFixture(entity, entity.GetComponent<CircleCollider2DComponent>());
        DestroyRigidbody(entity);
        m_PhysicsWorld2D = tempWorld;
        return mass;
    }

    glm::vec2 Physics2D::CalculateCenterOfMass(Entity entity)
    {
        if (!entity.HasComponent<Rigidbody2DComponent>())
            return { 0.0f, 0.0f };
        b2World* tempWorld = m_PhysicsWorld2D;
        m_PhysicsWorld2D = m_TemporaryWorld2D;

        CreateRigidbody(entity);
        if (entity.HasComponent<BoxCollider2DComponent>())
            CreateBoxCollider(entity);
        if (entity.HasComponent<CircleCollider2DComponent>())
            CreateCircleCollider(entity);
        glm::vec2 center = entity.GetComponent<Rigidbody2DComponent>().GetCenterOfMass();
        if (entity.HasComponent<BoxCollider2DComponent>())
            DestroyFixture(entity, entity.GetComponent<BoxCollider2DComponent>());
        if (entity.HasComponent<CircleCollider2DComponent>())
            DestroyFixture(entity, entity.GetComponent<CircleCollider2DComponent>());
        DestroyRigidbody(entity);
        m_PhysicsWorld2D = tempWorld;
        return center;
    }

} // namespace Crowny
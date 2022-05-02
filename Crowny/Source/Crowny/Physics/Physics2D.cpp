#include "cwpch.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Physics/Physics2D.h"

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
        CW_ENGINE_WARN("Collsion begin");
        b2WorldManifold manifold;
        contact->GetWorldManifold(&manifold);
        Collision2D col;
        col.Points.push_back(glm::vec2(manifold.points[0].x, manifold.points[0].y));
        col.Points.push_back(glm::vec2(manifold.points[1].x, manifold.points[1].y));
        Entity e1 = Entity((entt::entity)contact->GetFixtureA()->GetBody()->GetUserData().pointer, m_Scene);
        Entity e2 = Entity((entt::entity)contact->GetFixtureB()->GetBody()->GetUserData().pointer, m_Scene);
        col.Colliders.push_back(e1);
        col.Colliders.push_back(e2);
        if (e1.HasComponent<BoxCollider2DComponent>())
            e1.GetComponent<BoxCollider2DComponent>().OnCollisionBegin(col);
        if (e1.HasComponent<CircleCollider2DComponent>())
            e1.GetComponent<CircleCollider2DComponent>().OnCollisionBegin(col);
        if (e2.HasComponent<BoxCollider2DComponent>())
            e2.GetComponent<BoxCollider2DComponent>().OnCollisionBegin(col);
        if (e2.HasComponent<CircleCollider2DComponent>())
            e2.GetComponent<CircleCollider2DComponent>().OnCollisionBegin(col);
    }

    void ContactListener::EndContact(b2Contact* contact)
    {
        CW_ENGINE_WARN("Collsion end");
        b2WorldManifold manifold;
        contact->GetWorldManifold(&manifold);
        Collision2D col;
        col.Points.push_back(glm::vec2(manifold.points[0].x, manifold.points[0].y));
        col.Points.push_back(glm::vec2(manifold.points[1].x, manifold.points[1].y));
        Entity e1 = Entity((entt::entity)contact->GetFixtureA()->GetBody()->GetUserData().pointer, m_Scene);
        Entity e2 = Entity((entt::entity)contact->GetFixtureA()->GetBody()->GetUserData().pointer, m_Scene);
        col.Colliders.push_back(e1);
        col.Colliders.push_back(e2);
        if (e1.HasComponent<BoxCollider2DComponent>())
            e1.GetComponent<BoxCollider2DComponent>().OnCollisionEnd(col);
        if (e1.HasComponent<CircleCollider2DComponent>())
            e1.GetComponent<CircleCollider2DComponent>().OnCollisionEnd(col);
        if (e2.HasComponent<BoxCollider2DComponent>())
            e2.GetComponent<BoxCollider2DComponent>().OnCollisionEnd(col);
        if (e2.HasComponent<CircleCollider2DComponent>())
            e2.GetComponent<CircleCollider2DComponent>().OnCollisionEnd(col);
    }

    Physics2D::Physics2D()
    {
        m_Settings = CreateRef<Physics2DSettings>();
        for (uint32_t i = 0; i < m_MaskBits.size(); i++)
            m_MaskBits[i] = 0xFFFFFFFF;
    }

    Physics2D::~Physics2D()
    {
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
        bodyDef.userData.pointer = (uintptr_t)entity;
        bodyDef.bullet = rigidBody2D.GetContinuousCollisionDetection();
        bodyDef.linearDamping = rigidBody2D.GetGravityScale();
        bodyDef.angularDamping = rigidBody2D.GetAngularDrag();

        b2Body* body = m_PhysicsWorld2D->CreateBody(&bodyDef);
        rigidBody2D.RuntimeBody = body;
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
        boxShape.SetAsBox(b2d.Size.x * transform.Scale.x, b2d.Size.y * transform.Scale.y,
                          { b2d.Offset.x, b2d.Offset.y }, 0.0f);

        b2FixtureDef fixtureDef;
        fixtureDef.shape = &boxShape;

        uint32_t layerMask =
          entity.HasComponent<Rigidbody2DComponent>() ? entity.GetComponent<Rigidbody2DComponent>().GetLayerMask() : 0;
        fixtureDef.filter.maskBits = 1 << layerMask;
        fixtureDef.filter.categoryBits = Physics2D::Get().GetCategoryMask(layerMask);

        fixtureDef.density = b2d.Material.m_Density;
        fixtureDef.friction = b2d.Material.m_Friction;
        fixtureDef.restitution = b2d.Material.m_Restitution;
        fixtureDef.isSensor = b2d.IsTrigger;
        fixtureDef.restitutionThreshold = b2d.Material.m_RestitutionThreshold;
        if (entity.HasComponent<Rigidbody2DComponent>())
            b2d.RuntimeFixture = entity.GetComponent<Rigidbody2DComponent>().RuntimeBody->CreateFixture(&fixtureDef);
    }

    void Physics2D::CreateCircleCollider(Entity entity)
    {
        auto& transform = entity.GetTransform();
        auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();

        b2CircleShape circleShape;
        circleShape.m_p.Set(cc2d.Offset.y, cc2d.Offset.y);
        circleShape.m_radius = transform.Scale.x * cc2d.Radius;

        b2FixtureDef fixtureDef;
        fixtureDef.shape = &circleShape;

        uint32_t layerMask =
          entity.HasComponent<Rigidbody2DComponent>() ? entity.GetComponent<Rigidbody2DComponent>().GetLayerMask() : 0;
        fixtureDef.filter.maskBits = 1 << layerMask;
        fixtureDef.filter.categoryBits = Physics2D::Get().GetCategoryMask(layerMask);

        fixtureDef.isSensor = cc2d.IsTrigger;
        fixtureDef.density = cc2d.Material.m_Density;
        fixtureDef.friction = cc2d.Material.m_Friction;
        fixtureDef.restitution = cc2d.Material.m_Restitution;
        fixtureDef.restitutionThreshold = cc2d.Material.m_RestitutionThreshold;

        if (entity.HasComponent<Rigidbody2DComponent>())
            cc2d.RuntimeFixture = entity.GetComponent<Rigidbody2DComponent>().RuntimeBody->CreateFixture(&fixtureDef);
    }

    void Physics2D::DestroyRigidbody(Entity entity)
    {
        m_PhysicsWorld2D->DestroyBody(entity.GetComponent<Rigidbody2DComponent>().RuntimeBody);
    }

    void Physics2D::DestroyFixture(Entity entity, const Collider2D& collider)
    {
        entity.GetComponent<Rigidbody2DComponent>().RuntimeBody->DestroyFixture(collider.RuntimeFixture);
    }

    void Physics2D::Step(Timestep ts, Scene* scene)
    {
        auto view1 = scene->GetAllEntitiesWith<Rigidbody2DComponent>();
        for (auto e : view1)
        {
            Entity entity = { e, scene };
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

} // namespace Crowny
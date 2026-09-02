#include "cwpch.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Physics/Physics3D.h"
#include "Crowny/Scene/SceneManager.h"

#include <algorithm>
#include <cmath>

namespace Crowny
{
    namespace
    {
        auto ContactEndpointKey(const PhysicsContactEvent3D& event, bool first)
        {
            const PhysicsShape3DHandle shape = first ? event.ShapeA : event.ShapeB;
            const PhysicsBody3DHandle body = first ? event.BodyA : event.BodyB;
            return std::tuple(shape.Value, body.Value);
        }

        auto ContactPairKey(const PhysicsContactEvent3D& event)
        {
            return std::tuple(event.ShapeA.Value, event.BodyA.Value, event.ShapeB.Value, event.BodyB.Value);
        }

        auto ContactPointKey(const PhysicsContactPoint3D& point)
        {
            return std::tuple(point.Point.x, point.Point.y, point.Point.z, point.Normal.x, point.Normal.y, point.Normal.z, point.Separation,
                              point.NormalImpulse);
        }

        void SortAndDeduplicateContactPoints(SmallVector<PhysicsContactPoint3D, 4>& points)
        {
            std::sort(points.begin(), points.end(),
                      [](const PhysicsContactPoint3D& lhs, const PhysicsContactPoint3D& rhs) { return ContactPointKey(lhs) < ContactPointKey(rhs); });
            uint32_t write = 0;
            for (uint32_t read = 0; read < points.size(); ++read)
            {
                if (write != 0 && ContactPointKey(points[write - 1]) == ContactPointKey(points[read]))
                    continue;
                if (write != read)
                    points[write] = std::move(points[read]);
                ++write;
            }
            points.resize(write);
        }

        void CanonicalizeContactEvent(PhysicsContactEvent3D& event)
        {
            if (ContactEndpointKey(event, false) < ContactEndpointKey(event, true))
            {
                std::swap(event.BodyA, event.BodyB);
                std::swap(event.ShapeA, event.ShapeB);
                std::swap(event.ShapeUserDataA, event.ShapeUserDataB);
                std::swap(event.MaterialA, event.MaterialB);
                for (PhysicsContactPoint3D& point : event.Points)
                    point.Normal = -point.Normal;
            }
            SortAndDeduplicateContactPoints(event.Points);
        }

        void MergeContactEvent(PhysicsContactEvent3D& target, PhysicsContactEvent3D&& source)
        {
            target.IsTrigger = target.IsTrigger || source.IsTrigger;
            target.Points.reserve(target.Points.size() + source.Points.size());
            for (PhysicsContactPoint3D& point : source.Points)
                target.Points.push_back(std::move(point));
            SortAndDeduplicateContactPoints(target.Points);
        }
    } // namespace

    void NormalizePhysicsContactEvents3D(Vector<PhysicsContactEvent3D>& events)
    {
        for (PhysicsContactEvent3D& event : events)
            CanonicalizeContactEvent(event);

        std::sort(events.begin(), events.end(), [](const PhysicsContactEvent3D& lhs, const PhysicsContactEvent3D& rhs) {
            return std::tuple(ContactPairKey(lhs), lhs.Type) < std::tuple(ContactPairKey(rhs), rhs.Type);
        });

        size_t write = 0;
        for (size_t read = 0; read < events.size();)
        {
            const auto pair = ContactPairKey(events[read]);
            const PhysicsContactEventType3D type = events[read].Type;
            PhysicsContactEvent3D merged = std::move(events[read++]);
            while (read < events.size() && ContactPairKey(events[read]) == pair && events[read].Type == type)
                MergeContactEvent(merged, std::move(events[read++]));
            events[write++] = std::move(merged);
        }
        events.resize(write);

        write = 0;
        for (size_t read = 0; read < events.size();)
        {
            const auto pair = ContactPairKey(events[read]);
            size_t pairEnd = read + 1;
            bool hasTransition = events[read].Type != PhysicsContactEventType3D::Stay;
            size_t enter = events[read].Type == PhysicsContactEventType3D::Enter ? read : events.size();
            while (pairEnd < events.size() && ContactPairKey(events[pairEnd]) == pair)
            {
                hasTransition = hasTransition || events[pairEnd].Type != PhysicsContactEventType3D::Stay;
                if (events[pairEnd].Type == PhysicsContactEventType3D::Enter)
                    enter = pairEnd;
                ++pairEnd;
            }
            if (enter != events.size())
            {
                for (size_t index = read; index < pairEnd; ++index)
                {
                    if (events[index].Type == PhysicsContactEventType3D::Stay)
                        MergeContactEvent(events[enter], std::move(events[index]));
                }
            }
            for (; read < pairEnd; ++read)
            {
                if (hasTransition && events[read].Type == PhysicsContactEventType3D::Stay)
                    continue;
                if (write != read)
                    events[write] = std::move(events[read]);
                ++write;
            }
        }
        events.resize(write);
    }

    Physics3D::Physics3D()
    {
        m_Settings.DefaultMaterial = static_asset_cast<PhysicsMaterial3D>(AssetManager::TryGet()->CreateAssetHandle(CreateRef<PhysicsMaterial3D>()));
        m_Backend = MakeBackend(m_Settings.Backend);
    }

    Physics3D::~Physics3D() { StopSimulation(); }

    Scope<Physics3DBackend> Physics3D::MakeBackend(Physics3DBackendType backend) const
    {
        switch (backend)
        {
        case Physics3DBackendType::Box3D:
            return CreateBox3DBackend();
        case Physics3DBackendType::Jolt:
            return CreateJoltPhysicsBackend();
        case Physics3DBackendType::Bullet:
            return CreateBulletPhysicsBackend();
        default:
            return nullptr;
        }
    }

    bool Physics3D::IsBackendCompiled(Physics3DBackendType backend)
    {
        Scope<Physics3DBackend> candidate;
        switch (backend)
        {
        case Physics3DBackendType::Box3D:
            candidate = CreateBox3DBackend();
            break;
        case Physics3DBackendType::Jolt:
            candidate = CreateJoltPhysicsBackend();
            break;
        case Physics3DBackendType::Bullet:
            candidate = CreateBulletPhysicsBackend();
            break;
        }
        return candidate != nullptr;
    }

    bool Physics3D::SetBackend(Physics3DBackendType backend)
    {
        if (m_Settings.Backend == backend && m_Backend)
            return true;
        Scope<Physics3DBackend> candidate = MakeBackend(backend);
        if (!candidate)
        {
            CW_ENGINE_ERROR("Requested 3D physics backend is not compiled into this build");
            return false;
        }

        const bool restart = m_Simulating;
        if (restart)
            StopSimulation();
        m_Backend = std::move(candidate);
        m_Settings.Backend = backend;
        if (restart)
            return StartSimulation(m_ContactCallback);
        return true;
    }

    const char* Physics3D::GetBackendName() const { return m_Backend ? m_Backend->GetName() : "Unavailable"; }

    Physics3DCapability Physics3D::GetCapabilities() const { return m_Backend ? m_Backend->GetCapabilities() : Physics3DCapability::None; }

    bool Physics3D::Supports(Physics3DCapability capability) const { return HasCapability(GetCapabilities(), capability); }

    bool Physics3D::StartSimulation(PhysicsContactCallback3D callback)
    {
        if (m_Simulating)
            return true;
        if (!m_Backend)
        {
            CW_ENGINE_ERROR("The selected 3D physics backend is unavailable");
            return false;
        }
        m_ContactCallback = std::move(callback);
        m_Simulating = m_Backend->Initialize(m_Settings, m_ContactCallback);
        return m_Simulating;
    }

    void Physics3D::StopSimulation()
    {
        if (!m_Simulating || !m_Backend)
            return;
        m_Backend->Shutdown();
        m_Simulating = false;
    }

    void Physics3D::Step(Timestep timestep)
    {
        if (!m_Simulating)
            return;
        const float fixedTimestep = timestep.GetSeconds();
        if (!std::isfinite(fixedTimestep) || fixedTimestep <= 0.0f)
            return;
        m_Backend->Step(fixedTimestep, std::max(m_Settings.Substeps, 1u));
    }

    void Physics3D::SetSettings(const Physics3DSettings& settings)
    {
        Physics3DSettings normalized = settings;
        normalized.Substeps = std::max(normalized.Substeps, 1u);
        if (!normalized.DefaultMaterial.HasUUID())
            normalized.DefaultMaterial = m_Settings.DefaultMaterial;
        const AssetHandle<PhysicsMaterial3D> previousDefaultMaterial = m_Settings.DefaultMaterial;
        const AssetHandle<PhysicsMaterial3D> requestedDefaultMaterial = normalized.DefaultMaterial;
        if (normalized.Backend != m_Settings.Backend)
        {
            Scope<Physics3DBackend> candidate = MakeBackend(normalized.Backend);
            if (!candidate)
            {
                CW_ENGINE_ERROR("Requested 3D physics backend is not compiled into this build");
                return;
            }
            const bool restart = m_Simulating;
            if (restart)
                StopSimulation();
            m_Backend = std::move(candidate);
            m_Settings = normalized;
            m_Settings.DefaultMaterial = previousDefaultMaterial;
            SetDefaultMaterial(requestedDefaultMaterial);
            if (restart)
                StartSimulation(m_ContactCallback);
            return;
        }
        m_Settings = normalized;
        m_Settings.DefaultMaterial = previousDefaultMaterial;
        SetDefaultMaterial(requestedDefaultMaterial);
        if (m_Simulating)
            m_Backend->SetGravity(normalized.Gravity);
    }

    void Physics3D::SetGravity(const glm::vec3& gravity)
    {
        m_Settings.Gravity = gravity;
        if (m_Simulating)
            m_Backend->SetGravity(gravity);
    }

    void Physics3D::SetDefaultMaterial(const AssetHandle<PhysicsMaterial3D>& material)
    {
        if (!material.HasUUID())
            return;
        const AssetHandle<PhysicsMaterial3D> previous = m_Settings.DefaultMaterial;
        m_Settings.DefaultMaterial = material;

        if (!SceneManager::TryGet() || !SceneManager::TryGet()->GetActiveScene())
            return;
        Scene* scene = SceneManager::TryGet()->GetActiveScene().get();
        const auto replaceDefault = [&](Collider3D& collider) {
            if (!collider.GetMaterial().HasUUID() || collider.GetMaterial().GetUUID() == previous.GetUUID())
                collider.SetMaterial(material);
            else if (!collider.GetMaterial())
                collider.RefreshMaterial();
        };
        for (const entt::entity handle : scene->GetAllEntitiesWith<BoxCollider3DComponent>())
            replaceDefault(Entity(handle, scene).GetComponent<BoxCollider3DComponent>());
        for (const entt::entity handle : scene->GetAllEntitiesWith<SphereCollider3DComponent>())
            replaceDefault(Entity(handle, scene).GetComponent<SphereCollider3DComponent>());
        for (const entt::entity handle : scene->GetAllEntitiesWith<CapsuleCollider3DComponent>())
            replaceDefault(Entity(handle, scene).GetComponent<CapsuleCollider3DComponent>());
    }

    PhysicsBody3DHandle Physics3D::CreateBody(const PhysicsBody3DDesc& desc)
    {
        return m_Simulating ? m_Backend->CreateBody(desc) : PhysicsBody3DHandle{};
    }

    void Physics3D::DestroyBody(PhysicsBody3DHandle body)
    {
        if (m_Simulating && body)
            m_Backend->DestroyBody(body);
    }

    PhysicsShape3DHandle Physics3D::AddShape(PhysicsBody3DHandle body, const PhysicsShape3DDesc& desc)
    {
        return m_Simulating && body ? m_Backend->AddShape(body, desc) : PhysicsShape3DHandle{};
    }

    void Physics3D::RemoveShape(PhysicsBody3DHandle body, PhysicsShape3DHandle shape)
    {
        if (m_Simulating && body && shape)
            m_Backend->RemoveShape(body, shape);
    }

    void Physics3D::SetBodyTransform(PhysicsBody3DHandle body, const glm::vec3& position, const glm::quat& rotation, bool activate)
    {
        if (m_Simulating && body)
            m_Backend->SetBodyTransform(body, position, rotation, activate);
    }

    void Physics3D::GetBodyTransform(PhysicsBody3DHandle body, glm::vec3& position, glm::quat& rotation) const
    {
        if (m_Simulating && body)
            m_Backend->GetBodyTransform(body, position, rotation);
    }

    void Physics3D::MoveKinematic(PhysicsBody3DHandle body, const glm::vec3& position, const glm::quat& rotation, float timestep)
    {
        if (m_Simulating && body)
            m_Backend->MoveKinematic(body, position, rotation, timestep);
    }

    void Physics3D::SetLinearVelocity(PhysicsBody3DHandle body, const glm::vec3& velocity)
    {
        if (m_Simulating && body)
            m_Backend->SetLinearVelocity(body, velocity);
    }

    glm::vec3 Physics3D::GetLinearVelocity(PhysicsBody3DHandle body) const
    {
        return m_Simulating && body ? m_Backend->GetLinearVelocity(body) : glm::vec3(0.0f);
    }

    void Physics3D::SetAngularVelocity(PhysicsBody3DHandle body, const glm::vec3& velocity)
    {
        if (m_Simulating && body)
            m_Backend->SetAngularVelocity(body, velocity);
    }

    glm::vec3 Physics3D::GetAngularVelocity(PhysicsBody3DHandle body) const
    {
        return m_Simulating && body ? m_Backend->GetAngularVelocity(body) : glm::vec3(0.0f);
    }

    void Physics3D::AddForce(PhysicsBody3DHandle body, const glm::vec3& force, PhysicsForceMode3D mode)
    {
        if (m_Simulating && body)
            m_Backend->AddForce(body, force, nullptr, mode);
    }

    void Physics3D::AddForceAt(PhysicsBody3DHandle body, const glm::vec3& force, const glm::vec3& point, PhysicsForceMode3D mode)
    {
        if (m_Simulating && body)
            m_Backend->AddForce(body, force, &point, mode);
    }

    void Physics3D::AddTorque(PhysicsBody3DHandle body, const glm::vec3& torque, PhysicsForceMode3D mode)
    {
        if (m_Simulating && body)
            m_Backend->AddTorque(body, torque, mode);
    }

    void Physics3D::SetGravityScale(PhysicsBody3DHandle body, float scale)
    {
        if (m_Simulating && body)
            m_Backend->SetGravityScale(body, scale);
    }

    void Physics3D::SetDamping(PhysicsBody3DHandle body, float linear, float angular)
    {
        if (m_Simulating && body)
            m_Backend->SetDamping(body, linear, angular);
    }

    void Physics3D::SetAwake(PhysicsBody3DHandle body, bool awake)
    {
        if (m_Simulating && body)
            m_Backend->SetAwake(body, awake);
    }

    bool Physics3D::IsAwake(PhysicsBody3DHandle body) const { return m_Simulating && body && m_Backend->IsAwake(body); }

    void Physics3D::SetFilter(PhysicsBody3DHandle body, const PhysicsFilter3D& filter)
    {
        if (m_Simulating && body)
            m_Backend->SetFilter(body, filter);
    }

    void Physics3D::SetShapeMaterial(PhysicsShape3DHandle shape, const PhysicsMaterialData& material)
    {
        if (m_Simulating && shape)
            m_Backend->SetShapeMaterial(shape, material);
    }

    void Physics3D::SetShapeTrigger(PhysicsShape3DHandle shape, bool trigger)
    {
        if (m_Simulating && shape)
            m_Backend->SetShapeTrigger(shape, trigger);
    }

    PhysicsConstraint3DHandle Physics3D::CreateConstraint(const PhysicsConstraint3DDesc& desc)
    {
        return m_Simulating ? m_Backend->CreateConstraint(desc) : PhysicsConstraint3DHandle{};
    }

    void Physics3D::DestroyConstraint(PhysicsConstraint3DHandle constraint)
    {
        if (m_Simulating && constraint)
            m_Backend->DestroyConstraint(constraint);
    }

    Vector<PhysicsQueryHit3D> Physics3D::Raycast(const glm::vec3& origin, const glm::vec3& direction, float distance,
                                                 const PhysicsQueryFilter3D& filter) const
    {
        if (!m_Simulating || !Supports(Physics3DCapability::RayCasts) || distance <= 0.0f || glm::dot(direction, direction) <= 0.0f)
            return {};
        return m_Backend->Raycast(origin, direction, distance, filter);
    }

    Vector<PhysicsQueryHit3D> Physics3D::Sweep(const PhysicsShape3DDesc& shape, const glm::vec3& position, const glm::quat& rotation,
                                               const glm::vec3& direction, float distance, const PhysicsQueryFilter3D& filter) const
    {
        if (!m_Simulating || !Supports(Physics3DCapability::ShapeCasts) || distance <= 0.0f || glm::dot(direction, direction) <= 0.0f)
            return {};
        return m_Backend->Sweep(shape, position, rotation, direction, distance, filter);
    }

    Vector<PhysicsQueryHit3D> Physics3D::Overlap(const PhysicsShape3DDesc& shape, const glm::vec3& position, const glm::quat& rotation,
                                                 const PhysicsQueryFilter3D& filter) const
    {
        if (!m_Simulating || !Supports(Physics3DCapability::Overlaps))
            return {};
        return m_Backend->Overlap(shape, position, rotation, filter);
    }
} // namespace Crowny

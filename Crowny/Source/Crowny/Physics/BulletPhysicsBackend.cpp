#include "cwpch.h"

#include "Crowny/Physics/Physics3D.h"

#if defined(CW_PHYSICS_BULLET)
#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>
#include <btBulletDynamicsCommon.h>

#include <algorithm>

#include <glm/gtc/constants.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Crowny
{
    namespace
    {
        btVector3 ToBullet(const glm::vec3& value) { return { value.x, value.y, value.z }; }
        btQuaternion ToBullet(const glm::quat& value) { return { value.x, value.y, value.z, value.w }; }
        glm::vec3 FromBullet(const btVector3& value) { return { value.x(), value.y(), value.z() }; }
        glm::quat FromBullet(const btQuaternion& value) { return { value.w(), value.x(), value.y(), value.z() }; }

        btTransform ToBullet(const glm::vec3& position, const glm::quat& rotation) { return btTransform(ToBullet(rotation), ToBullet(position)); }

        short CollisionGroup(const PhysicsFilter3D& filter) { return static_cast<short>(1u << std::min(filter.Layer, 15u)); }

        short CollisionMask(const PhysicsFilter3D& filter) { return static_cast<short>(filter.Mask & 0xFFFF); }

        struct BulletContactKey
        {
            uint64_t A = 0;
            uint64_t B = 0;
            bool operator==(const BulletContactKey& other) const { return A == other.A && B == other.B; }
        };

        struct BulletContactKeyHash
        {
            size_t operator()(const BulletContactKey& value) const
            {
                const size_t a = std::hash<uint64_t>{}(value.A);
                const size_t b = std::hash<uint64_t>{}(value.B);
                return a ^ (b + 0x9e3779b97f4a7c15ull + (a << 6) + (a >> 2));
            }
        };

        BulletContactKey MakeKey(PhysicsShape3DHandle first, PhysicsShape3DHandle second)
        {
            return first.Value < second.Value ? BulletContactKey{ first.Value, second.Value } : BulletContactKey{ second.Value, first.Value };
        }
    } // namespace

    class BulletPhysicsBackend final : public Physics3DBackend
    {
    public:
        ~BulletPhysicsBackend() override { Shutdown(); }

        Physics3DBackendType GetType() const override { return Physics3DBackendType::Bullet; }
        const char* GetName() const override { return "Bullet 3.25"; }
        Physics3DCapability GetCapabilities() const override
        {
            return Physics3DCapability::RigidBodies | Physics3DCapability::PrimitiveShapes | Physics3DCapability::ConvexShapes |
                   Physics3DCapability::TriangleMeshes | Physics3DCapability::HeightFields | Physics3DCapability::CompoundShapes |
                   Physics3DCapability::Sensors | Physics3DCapability::ContinuousCollision | Physics3DCapability::RayCasts |
                   Physics3DCapability::ShapeCasts | Physics3DCapability::Overlaps | Physics3DCapability::Constraints | Physics3DCapability::Motors |
                   Physics3DCapability::Springs;
        }

        bool Initialize(const Physics3DSettings& settings, PhysicsContactCallback3D callback) override
        {
            Shutdown();
            m_PreviousContactAddedCallback = gContactAddedCallback;
            s_ActiveBackend = this;
            gContactAddedCallback = &ContactAdded;
            m_CollisionConfiguration = CreateScope<btDefaultCollisionConfiguration>();
            m_Dispatcher = CreateScope<btCollisionDispatcher>(m_CollisionConfiguration.get());
            m_Broadphase = CreateScope<btDbvtBroadphase>();
            m_Solver = CreateScope<btSequentialImpulseConstraintSolver>();
            m_World = CreateScope<btDiscreteDynamicsWorld>(m_Dispatcher.get(), m_Broadphase.get(), m_Solver.get(), m_CollisionConfiguration.get());
            m_Gravity = settings.Gravity;
            m_World->setGravity(ToBullet(settings.Gravity));
            m_Callback = std::move(callback);
            return true;
        }

        void Shutdown() override
        {
            if (m_World)
            {
                for (auto& [handle, constraint] : m_Constraints)
                    m_World->removeConstraint(constraint.get());
                for (auto& [handle, body] : m_Bodies)
                    m_World->removeRigidBody(body.Body.get());
            }
            m_Constraints.clear();
            m_BodyLookup.clear();
            m_Bodies.clear();
            m_Shapes.clear();
            m_ActiveContacts.clear();
            m_ContactEvents.clear();
            m_World.reset();
            m_Solver.reset();
            m_Broadphase.reset();
            m_Dispatcher.reset();
            m_CollisionConfiguration.reset();
            m_Callback = nullptr;
            if (s_ActiveBackend == this)
            {
                s_ActiveBackend = nullptr;
                if (gContactAddedCallback == &ContactAdded)
                    gContactAddedCallback = m_PreviousContactAddedCallback;
            }
            m_PreviousContactAddedCallback = nullptr;
        }

        void Step(float timestep, uint32_t substeps) override
        {
            if (!m_World || timestep <= 0.0f)
                return;
            m_World->stepSimulation(timestep, static_cast<int>(std::max(substeps, 1u)), timestep / std::max(substeps, 1u));
            DispatchContacts();
        }

        void SetGravity(const glm::vec3& gravity) override
        {
            m_Gravity = gravity;
            if (!m_World)
                return;
            m_World->setGravity(ToBullet(gravity));
            for (auto& [handle, body] : m_Bodies)
                body.Body->setGravity(ToBullet(gravity * body.Desc.GravityScale));
        }

        PhysicsBody3DHandle CreateBody(const PhysicsBody3DDesc& desc) override
        {
            if (!m_World)
                return {};
            const PhysicsBody3DHandle handle{ NextHandle() };
            BodyRecord record;
            record.Desc = desc;
            record.Compound = CreateScope<btCompoundShape>(true);
            record.MotionState = CreateScope<btDefaultMotionState>(ToBullet(desc.Position, desc.Rotation));
            const btScalar mass = desc.Type == PhysicsBodyType3D::Dynamic ? std::max(desc.Mass, 0.001f) : 0.0f;
            btRigidBody::btRigidBodyConstructionInfo info(mass, record.MotionState.get(), record.Compound.get());
            info.m_linearDamping = std::max(desc.LinearDamping, 0.0f);
            info.m_angularDamping = std::max(desc.AngularDamping, 0.0f);
            record.Body = CreateScope<btRigidBody>(info);
            record.Body->setUserPointer(reinterpret_cast<void*>(handle.Value));
            record.Body->setLinearVelocity(ToBullet(desc.LinearVelocity));
            record.Body->setAngularVelocity(ToBullet(desc.AngularVelocity));
            record.Body->setGravity(ToBullet(m_Gravity * desc.GravityScale));
            record.Body->setAngularFactor(
              btVector3(desc.LockRotationX ? 0.0f : 1.0f, desc.LockRotationY ? 0.0f : 1.0f, desc.LockRotationZ ? 0.0f : 1.0f));
            if (desc.Type == PhysicsBodyType3D::Kinematic)
            {
                record.Body->setCollisionFlags(record.Body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
                record.Body->setActivationState(DISABLE_DEACTIVATION);
            }
            else if (!desc.AllowSleep)
                record.Body->setActivationState(DISABLE_DEACTIVATION);
            if (desc.Continuous)
            {
                record.Body->setCcdMotionThreshold(0.001f);
                record.Body->setCcdSweptSphereRadius(0.01f);
            }
            if (!desc.StartAwake && desc.Type == PhysicsBodyType3D::Dynamic)
                record.Body->forceActivationState(ISLAND_SLEEPING);

            btRigidBody* native = record.Body.get();
            m_World->addRigidBody(native, CollisionGroup(desc.Filter), CollisionMask(desc.Filter));
            m_BodyLookup[native] = handle;
            m_Bodies.emplace(handle, std::move(record));
            return handle;
        }

        void DestroyBody(PhysicsBody3DHandle body) override
        {
            auto bodyIt = m_Bodies.find(body);
            if (bodyIt == m_Bodies.end())
                return;
            Vector<PhysicsShape3DHandle> shapes;
            for (const auto& [handle, shape] : m_Shapes)
            {
                if (shape.Body == body)
                    shapes.push_back(handle);
            }
            for (PhysicsShape3DHandle shape : shapes)
                RemoveShape(body, shape);
            m_World->removeRigidBody(bodyIt->second.Body.get());
            m_BodyLookup.erase(bodyIt->second.Body.get());
            m_Bodies.erase(bodyIt);
        }

        PhysicsShape3DHandle AddShape(PhysicsBody3DHandle body, const PhysicsShape3DDesc& desc) override
        {
            BodyRecord* bodyRecord = FindBody(body);
            if (!bodyRecord)
                return {};
            const PhysicsShape3DHandle handle{ NextHandle() };
            ShapeRecord shape;
            shape.Body = body;
            shape.Desc = desc;
            if ((desc.Type == PhysicsShapeType3D::TriangleMesh || desc.Type == PhysicsShapeType3D::HeightField) &&
                bodyRecord->Desc.Type != PhysicsBodyType3D::Static)
            {
                CW_ENGINE_ERROR("Bullet triangle meshes and height fields require a static body");
                return {};
            }
            shape.Shape = CreateShape(shape);
            if (!shape.Shape)
                return {};
            shape.Shape->setUserPointer(reinterpret_cast<void*>(handle.Value));
            bodyRecord->Compound->addChildShape(ToBullet(desc.LocalPosition, desc.LocalRotation), shape.Shape.get());
            m_Shapes.emplace(handle, std::move(shape));
            UpdateBodyMassAndFlags(*bodyRecord);
            return handle;
        }

        void RemoveShape(PhysicsBody3DHandle body, PhysicsShape3DHandle shape) override
        {
            auto shapeIt = m_Shapes.find(shape);
            BodyRecord* bodyRecord = FindBody(body);
            if (!bodyRecord || shapeIt == m_Shapes.end() || shapeIt->second.Body != body)
                return;
            bodyRecord->Compound->removeChildShape(shapeIt->second.Shape.get());
            m_Shapes.erase(shapeIt);
            UpdateBodyMassAndFlags(*bodyRecord);
        }

        void SetBodyTransform(PhysicsBody3DHandle body, const glm::vec3& position, const glm::quat& rotation, bool activate) override
        {
            if (BodyRecord* record = FindBody(body))
            {
                const btTransform transform = ToBullet(position, rotation);
                record->Body->setWorldTransform(transform);
                record->MotionState->setWorldTransform(transform);
                if (activate)
                    record->Body->activate(true);
                m_World->updateSingleAabb(record->Body.get());
            }
        }

        void GetBodyTransform(PhysicsBody3DHandle body, glm::vec3& position, glm::quat& rotation) const override
        {
            if (const BodyRecord* record = FindBody(body))
            {
                const btTransform& transform = record->Body->getWorldTransform();
                position = FromBullet(transform.getOrigin());
                rotation = FromBullet(transform.getRotation());
            }
        }

        void MoveKinematic(PhysicsBody3DHandle body, const glm::vec3& position, const glm::quat& rotation, float timestep) override
        {
            BodyRecord* record = FindBody(body);
            if (!record || timestep <= 0.0f)
                return;
            const btTransform current = record->Body->getWorldTransform();
            const btTransform target = ToBullet(position, rotation);
            record->Body->setLinearVelocity((target.getOrigin() - current.getOrigin()) / timestep);
            btQuaternion delta = target.getRotation() * current.getRotation().inverse();
            btVector3 axis;
            btScalar angle;
            axis = delta.getAxis();
            angle = delta.getAngle();
            record->Body->setAngularVelocity(axis * angle / timestep);
            record->MotionState->setWorldTransform(target);
            record->Body->setWorldTransform(target);
            record->Body->activate(true);
        }

        void SetLinearVelocity(PhysicsBody3DHandle body, const glm::vec3& velocity) override
        {
            if (BodyRecord* record = FindBody(body))
                record->Body->setLinearVelocity(ToBullet(velocity));
        }

        glm::vec3 GetLinearVelocity(PhysicsBody3DHandle body) const override
        {
            const BodyRecord* record = FindBody(body);
            return record ? FromBullet(record->Body->getLinearVelocity()) : glm::vec3(0.0f);
        }

        void SetAngularVelocity(PhysicsBody3DHandle body, const glm::vec3& velocity) override
        {
            if (BodyRecord* record = FindBody(body))
                record->Body->setAngularVelocity(ToBullet(velocity));
        }

        glm::vec3 GetAngularVelocity(PhysicsBody3DHandle body) const override
        {
            const BodyRecord* record = FindBody(body);
            return record ? FromBullet(record->Body->getAngularVelocity()) : glm::vec3(0.0f);
        }

        void AddForce(PhysicsBody3DHandle body, const glm::vec3& force, const glm::vec3* point, PhysicsForceMode3D mode) override
        {
            BodyRecord* record = FindBody(body);
            if (!record)
                return;
            glm::vec3 value = force;
            const float mass = record->Body->getInvMass() > 0.0f ? 1.0f / record->Body->getInvMass() : 0.0f;
            if (mode == PhysicsForceMode3D::Acceleration || mode == PhysicsForceMode3D::VelocityChange)
                value *= mass;
            if (mode == PhysicsForceMode3D::Impulse || mode == PhysicsForceMode3D::VelocityChange)
            {
                if (point)
                    record->Body->applyImpulse(ToBullet(value), ToBullet(*point) - record->Body->getCenterOfMassPosition());
                else
                    record->Body->applyCentralImpulse(ToBullet(value));
            }
            else if (point)
                record->Body->applyForce(ToBullet(value), ToBullet(*point) - record->Body->getCenterOfMassPosition());
            else
                record->Body->applyCentralForce(ToBullet(value));
            record->Body->activate(true);
        }

        void AddTorque(PhysicsBody3DHandle body, const glm::vec3& torque, PhysicsForceMode3D mode) override
        {
            BodyRecord* record = FindBody(body);
            if (!record)
                return;
            glm::vec3 value = torque;
            if (mode == PhysicsForceMode3D::Acceleration || mode == PhysicsForceMode3D::VelocityChange)
            {
                const float mass = record->Body->getInvMass() > 0.0f ? 1.0f / record->Body->getInvMass() : 0.0f;
                value *= mass;
            }
            if (mode == PhysicsForceMode3D::Impulse || mode == PhysicsForceMode3D::VelocityChange)
                record->Body->applyTorqueImpulse(ToBullet(value));
            else
                record->Body->applyTorque(ToBullet(value));
            record->Body->activate(true);
        }

        void SetGravityScale(PhysicsBody3DHandle body, float scale) override
        {
            if (BodyRecord* record = FindBody(body))
            {
                record->Desc.GravityScale = scale;
                record->Body->setGravity(ToBullet(m_Gravity * scale));
            }
        }

        void SetDamping(PhysicsBody3DHandle body, float linear, float angular) override
        {
            if (BodyRecord* record = FindBody(body))
                record->Body->setDamping(std::max(linear, 0.0f), std::max(angular, 0.0f));
        }

        void SetAwake(PhysicsBody3DHandle body, bool awake) override
        {
            if (BodyRecord* record = FindBody(body))
            {
                if (awake)
                    record->Body->activate(true);
                else
                    record->Body->setActivationState(ISLAND_SLEEPING);
            }
        }

        bool IsAwake(PhysicsBody3DHandle body) const override
        {
            const BodyRecord* record = FindBody(body);
            return record && record->Body->isActive();
        }

        void SetFilter(PhysicsBody3DHandle body, const PhysicsFilter3D& filter) override
        {
            BodyRecord* record = FindBody(body);
            if (!record)
                return;
            record->Desc.Filter = filter;
            for (auto& [handle, shape] : m_Shapes)
            {
                if (shape.Body == body)
                    shape.Desc.Filter = filter;
            }
            m_World->removeRigidBody(record->Body.get());
            m_World->addRigidBody(record->Body.get(), CollisionGroup(filter), CollisionMask(filter));
        }

        void SetShapeMaterial(PhysicsShape3DHandle shape, const PhysicsMaterialData& material) override
        {
            auto it = m_Shapes.find(shape);
            if (it == m_Shapes.end())
                return;
            it->second.Desc.Material = NormalizePhysicsMaterialData(material);
            if (BodyRecord* body = FindBody(it->second.Body))
                UpdateBodyMassAndFlags(*body);
        }

        void SetShapeTrigger(PhysicsShape3DHandle shape, bool trigger) override
        {
            auto it = m_Shapes.find(shape);
            if (it == m_Shapes.end())
                return;
            it->second.Desc.IsTrigger = trigger;
            if (BodyRecord* body = FindBody(it->second.Body))
                UpdateBodyMassAndFlags(*body);
        }

        PhysicsConstraint3DHandle CreateConstraint(const PhysicsConstraint3DDesc& desc) override
        {
            BodyRecord* bodyA = FindBody(desc.BodyA);
            BodyRecord* bodyB = FindBody(desc.BodyB);
            if (!bodyA || !bodyB)
                return {};
            Scope<btTypedConstraint> constraint;
            const btTransform frameA = ConstraintFrame(desc.AnchorA, desc.AxisA, desc.Type);
            const btTransform frameB = ConstraintFrame(desc.AnchorB, desc.AxisB, desc.Type);
            switch (desc.Type)
            {
            case PhysicsConstraintType3D::Fixed:
                constraint = CreateScope<btFixedConstraint>(*bodyA->Body, *bodyB->Body, frameA, frameB);
                break;
            case PhysicsConstraintType3D::Point:
                constraint = CreateScope<btPoint2PointConstraint>(*bodyA->Body, *bodyB->Body, ToBullet(desc.AnchorA), ToBullet(desc.AnchorB));
                break;
            case PhysicsConstraintType3D::Hinge: {
                auto hinge = CreateScope<btHingeConstraint>(*bodyA->Body, *bodyB->Body, frameA, frameB);
                if (desc.EnableLimits)
                    hinge->setLimit(desc.MinimumLimit, desc.MaximumLimit);
                if (desc.EnableMotor)
                    hinge->enableAngularMotor(true, desc.MotorTargetVelocity, desc.MaximumMotorForce);
                constraint = std::move(hinge);
                break;
            }
            case PhysicsConstraintType3D::Slider: {
                auto slider = CreateScope<btSliderConstraint>(*bodyA->Body, *bodyB->Body, frameA, frameB, true);
                if (desc.EnableLimits)
                {
                    slider->setLowerLinLimit(desc.MinimumLimit);
                    slider->setUpperLinLimit(desc.MaximumLimit);
                }
                if (desc.EnableMotor)
                {
                    slider->setPoweredLinMotor(true);
                    slider->setTargetLinMotorVelocity(desc.MotorTargetVelocity);
                    slider->setMaxLinMotorForce(desc.MaximumMotorForce);
                }
                constraint = std::move(slider);
                break;
            }
            case PhysicsConstraintType3D::ConeTwist: {
                auto cone = CreateScope<btConeTwistConstraint>(*bodyA->Body, *bodyB->Body, frameA, frameB);
                if (desc.EnableLimits)
                    cone->setLimit(std::abs(desc.MaximumLimit), std::abs(desc.MaximumLimit),
                                   std::max(std::abs(desc.MinimumLimit), std::abs(desc.MaximumLimit)));
                if (desc.EnableMotor)
                {
                    cone->enableMotor(true);
                    cone->setMaxMotorImpulse(desc.MaximumMotorForce);
                }
                constraint = std::move(cone);
                break;
            }
            case PhysicsConstraintType3D::Distance:
            case PhysicsConstraintType3D::Spring: {
                auto spring = CreateScope<btGeneric6DofSpring2Constraint>(*bodyA->Body, *bodyB->Body, frameA, frameB);
                spring->setLinearLowerLimit(btVector3(desc.MinimumLimit, 0.0f, 0.0f));
                spring->setLinearUpperLimit(btVector3(desc.MaximumLimit, 0.0f, 0.0f));
                spring->setAngularLowerLimit(btVector3(0.0f, 0.0f, 0.0f));
                spring->setAngularUpperLimit(btVector3(0.0f, 0.0f, 0.0f));
                if (desc.Type == PhysicsConstraintType3D::Spring || desc.EnableSpring)
                {
                    spring->enableSpring(0, true);
                    spring->setStiffness(0, desc.Frequency * desc.Frequency);
                    spring->setDamping(0, desc.Damping);
                }
                if (desc.EnableMotor)
                {
                    spring->enableMotor(0, true);
                    spring->setTargetVelocity(0, desc.MotorTargetVelocity);
                    spring->setMaxMotorForce(0, desc.MaximumMotorForce);
                }
                constraint = std::move(spring);
                break;
            }
            }
            if (!constraint)
                return {};
            constraint->setBreakingImpulseThreshold(std::min(desc.BreakForce, desc.BreakTorque));
            const PhysicsConstraint3DHandle handle{ NextHandle() };
            m_World->addConstraint(constraint.get(), !desc.EnableCollision);
            m_Constraints.emplace(handle.Value, std::move(constraint));
            return handle;
        }

        void DestroyConstraint(PhysicsConstraint3DHandle constraint) override
        {
            auto it = m_Constraints.find(constraint.Value);
            if (it == m_Constraints.end())
                return;
            m_World->removeConstraint(it->second.get());
            m_Constraints.erase(it);
        }

        Vector<PhysicsQueryHit3D> Raycast(const glm::vec3& origin, const glm::vec3& direction, float distance,
                                          const PhysicsQueryFilter3D& filter) const override
        {
            const btVector3 from = ToBullet(origin);
            const btVector3 to = from + ToBullet(glm::normalize(direction) * distance);
            FilteredRayCallback callback(*this, filter, from, to);
            m_World->rayTest(from, to, callback);
            if (!callback.hasHit())
                return {};
            PhysicsQueryHit3D hit = MakeHit(callback.m_collisionObject, nullptr, FromBullet(callback.m_hitPointWorld),
                                            FromBullet(callback.m_hitNormalWorld), callback.m_closestHitFraction, distance);
            return hit.Shape ? Vector<PhysicsQueryHit3D>{ hit } : Vector<PhysicsQueryHit3D>{};
        }

        Vector<PhysicsQueryHit3D> Sweep(const PhysicsShape3DDesc& shape, const glm::vec3& position, const glm::quat& rotation,
                                        const glm::vec3& direction, float distance, const PhysicsQueryFilter3D& filter) const override
        {
            ShapeRecord temporary;
            temporary.Desc = shape;
            temporary.Shape = CreateShape(temporary);
            if (temporary.Shape == nullptr || !temporary.Shape->isConvex())
                return {};
            auto* convex = static_cast<btConvexShape*>(temporary.Shape.get());
            const btTransform from = ToBullet(position + rotation * shape.LocalPosition, rotation * shape.LocalRotation);
            btTransform to = from;
            to.setOrigin(to.getOrigin() + ToBullet(glm::normalize(direction) * distance));
            FilteredConvexCallback callback(*this, filter, from.getOrigin(), to.getOrigin());
            m_World->convexSweepTest(convex, from, to, callback);
            if (!callback.hasHit())
                return {};
            PhysicsQueryHit3D hit = MakeHit(callback.m_hitCollisionObject, nullptr, FromBullet(callback.m_hitPointWorld),
                                            FromBullet(callback.m_hitNormalWorld), callback.m_closestHitFraction, distance);
            return hit.Shape ? Vector<PhysicsQueryHit3D>{ hit } : Vector<PhysicsQueryHit3D>{};
        }

        Vector<PhysicsQueryHit3D> Overlap(const PhysicsShape3DDesc& shape, const glm::vec3& position, const glm::quat& rotation,
                                          const PhysicsQueryFilter3D& filter) const override
        {
            ShapeRecord temporary;
            temporary.Desc = shape;
            temporary.Shape = CreateShape(temporary);
            if (!temporary.Shape)
                return {};
            btCollisionObject object;
            object.setCollisionShape(temporary.Shape.get());
            object.setWorldTransform(ToBullet(position + rotation * shape.LocalPosition, rotation * shape.LocalRotation));
            OverlapResult callback(*this, filter);
            m_World->contactTest(&object, callback);
            return callback.Hits;
        }

    private:
        struct BodyRecord
        {
            PhysicsBody3DDesc Desc;
            Scope<btCompoundShape> Compound;
            Scope<btDefaultMotionState> MotionState;
            Scope<btRigidBody> Body;
        };

        struct ShapeRecord
        {
            PhysicsBody3DHandle Body;
            PhysicsShape3DDesc Desc;
            Scope<btTriangleMesh> TriangleMesh;
            Scope<btCollisionShape> Shape;
        };

        struct ContactRecord
        {
            PhysicsContactEvent3D Event;
        };

        class FilteredRayCallback final : public btCollisionWorld::ClosestRayResultCallback
        {
        public:
            FilteredRayCallback(const BulletPhysicsBackend& backend, const PhysicsQueryFilter3D& filter, const btVector3& from, const btVector3& to)
              : ClosestRayResultCallback(from, to), Backend(backend), Filter(filter)
            {
                m_collisionFilterMask = static_cast<short>(filter.LayerMask & 0xFFFF);
            }

            bool needsCollision(btBroadphaseProxy* proxy) const override
            {
                if (!ClosestRayResultCallback::needsCollision(proxy))
                    return false;
                auto* object = static_cast<btCollisionObject*>(proxy->m_clientObject);
                return Backend.AcceptObject(object, Filter);
            }

            const BulletPhysicsBackend& Backend;
            PhysicsQueryFilter3D Filter;
        };

        static bool ContactAdded(btManifoldPoint& point, const btCollisionObjectWrapper* firstWrapper, CW_MAYBE_UNUSED int firstPart,
                                 CW_MAYBE_UNUSED int firstIndex, const btCollisionObjectWrapper* secondWrapper, CW_MAYBE_UNUSED int secondPart,
                                 CW_MAYBE_UNUSED int secondIndex)
        {
            if (!s_ActiveBackend)
                return false;
            const PhysicsMaterialData* first = s_ActiveBackend->FindMaterial(firstWrapper);
            const PhysicsMaterialData* second = s_ActiveBackend->FindMaterial(secondWrapper);
            if (!first || !second)
            {
                if (s_ActiveBackend->m_PreviousContactAddedCallback && s_ActiveBackend->m_PreviousContactAddedCallback != &ContactAdded)
                    return s_ActiveBackend->m_PreviousContactAddedCallback(point, firstWrapper, firstPart, firstIndex, secondWrapper, secondPart,
                                                                          secondIndex);
                return false;
            }
            point.m_combinedFriction = CombinePhysicsMaterialValue(first->Friction, first->FrictionCombine, second->Friction,
                                                                   second->FrictionCombine);
            point.m_combinedRestitution = CombinePhysicsMaterialValue(first->Restitution, first->RestitutionCombine, second->Restitution,
                                                                      second->RestitutionCombine);
            return true;
        }

        const PhysicsMaterialData* FindMaterial(const btCollisionObjectWrapper* wrapper) const
        {
            if (!wrapper || !wrapper->getCollisionShape())
                return nullptr;
            const uint64_t handle = reinterpret_cast<uint64_t>(wrapper->getCollisionShape()->getUserPointer());
            const auto found = m_Shapes.find(PhysicsShape3DHandle{ handle });
            return found == m_Shapes.end() ? nullptr : &found->second.Desc.Material;
        }

        class FilteredConvexCallback final : public btCollisionWorld::ClosestConvexResultCallback
        {
        public:
            FilteredConvexCallback(const BulletPhysicsBackend& backend, const PhysicsQueryFilter3D& filter, const btVector3& from,
                                   const btVector3& to)
              : ClosestConvexResultCallback(from, to), Backend(backend), Filter(filter)
            {
                m_collisionFilterMask = static_cast<short>(filter.LayerMask & 0xFFFF);
            }

            bool needsCollision(btBroadphaseProxy* proxy) const override
            {
                if (!ClosestConvexResultCallback::needsCollision(proxy))
                    return false;
                auto* object = static_cast<btCollisionObject*>(proxy->m_clientObject);
                return Backend.AcceptObject(object, Filter);
            }

            const BulletPhysicsBackend& Backend;
            PhysicsQueryFilter3D Filter;
        };

        class OverlapResult final : public btCollisionWorld::ContactResultCallback
        {
        public:
            OverlapResult(const BulletPhysicsBackend& backend, const PhysicsQueryFilter3D& filter) : Backend(backend), Filter(filter)
            {
                m_collisionFilterMask = static_cast<short>(filter.LayerMask & 0xFFFF);
            }

            btScalar addSingleResult(btManifoldPoint&, const btCollisionObjectWrapper* objectA, int, int indexA,
                                     const btCollisionObjectWrapper* objectB, int, int indexB) override
            {
                const btCollisionObject* hitObject = objectA->getCollisionObject();
                int childIndex = indexA;
                if (!Backend.AcceptObject(hitObject, Filter))
                {
                    hitObject = objectB->getCollisionObject();
                    childIndex = indexB;
                }

                if (!Backend.AcceptObject(hitObject, Filter))
                    return 1.0f;
                PhysicsQueryHit3D hit = Backend.MakeHit(hitObject, nullptr, {}, {}, 0.0f, 0.0f, childIndex);
                if (hit.Shape && std::none_of(Hits.begin(), Hits.end(), [&](const PhysicsQueryHit3D& value) { return value.Shape == hit.Shape; }))
                    Hits.push_back(hit);
                return 1.0f;
            }

            const BulletPhysicsBackend& Backend;
            PhysicsQueryFilter3D Filter;
            Vector<PhysicsQueryHit3D> Hits;
        };

        uint64_t NextHandle() { return m_NextHandle++; }

        BodyRecord* FindBody(PhysicsBody3DHandle body)
        {
            auto it = m_Bodies.find(body);
            return it == m_Bodies.end() ? nullptr : &it->second;
        }

        const BodyRecord* FindBody(PhysicsBody3DHandle body) const
        {
            auto it = m_Bodies.find(body);
            return it == m_Bodies.end() ? nullptr : &it->second;
        }

        Scope<btCollisionShape> CreateShape(ShapeRecord& record) const
        {
            const PhysicsShape3DDesc& desc = record.Desc;
            switch (desc.Type)
            {
            case PhysicsShapeType3D::Box:
                return CreateScope<btBoxShape>(ToBullet(glm::max(glm::abs(desc.HalfExtents), glm::vec3(0.001f))));
            case PhysicsShapeType3D::Sphere:
                return CreateScope<btSphereShape>(std::max(desc.Radius, 0.001f));
            case PhysicsShapeType3D::Capsule:
                return CreateScope<btCapsuleShape>(std::max(desc.Radius, 0.001f), std::max(desc.Height - 2.0f * desc.Radius, 0.001f));
            case PhysicsShapeType3D::ConvexHull: {
                if (desc.Vertices.size() < 4)
                    return nullptr;
                auto hull = CreateScope<btConvexHullShape>();
                for (const glm::vec3& point : desc.Vertices)
                    hull->addPoint(ToBullet(point), false);
                hull->recalcLocalAabb();
                hull->optimizeConvexHull();
                return hull;
            }
            case PhysicsShapeType3D::TriangleMesh: {
                if (desc.Indices.size() < 3 || desc.Indices.size() % 3 != 0)
                    return nullptr;
                record.TriangleMesh = CreateScope<btTriangleMesh>();
                for (size_t i = 0; i < desc.Indices.size(); i += 3)
                {
                    if (desc.Indices[i] >= desc.Vertices.size() || desc.Indices[i + 1] >= desc.Vertices.size() ||
                        desc.Indices[i + 2] >= desc.Vertices.size())
                        return nullptr;
                    record.TriangleMesh->addTriangle(ToBullet(desc.Vertices[desc.Indices[i]]), ToBullet(desc.Vertices[desc.Indices[i + 1]]),
                                                     ToBullet(desc.Vertices[desc.Indices[i + 2]]));
                }
                return CreateScope<btBvhTriangleMeshShape>(record.TriangleMesh.get(), true, true);
            }
            case PhysicsShapeType3D::HeightField: {
                if (desc.HeightFieldRows < 2 || desc.HeightFieldColumns < 2 ||
                    desc.Heights.size() != size_t(desc.HeightFieldRows) * desc.HeightFieldColumns)
                    return nullptr;
                const auto range = std::minmax_element(desc.Heights.begin(), desc.Heights.end());
                auto field = CreateScope<btHeightfieldTerrainShape>(static_cast<int>(desc.HeightFieldColumns), static_cast<int>(desc.HeightFieldRows),
                                                                    desc.Heights.data(), 1.0f, *range.first, *range.second, 1, PHY_FLOAT, false);
                field->setLocalScaling(ToBullet(desc.HeightFieldScale));
                return field;
            }
            }
            return nullptr;
        }

        void UpdateBodyMassAndFlags(BodyRecord& body)
        {
            float mass = 0.0f;
            float friction = 0.0f;
            float restitution = 0.0f;
            bool hasTrigger = false;
            uint32_t count = 0;
            for (const auto& [handle, shape] : m_Shapes)
            {
                if (shape.Body.Value != reinterpret_cast<uint64_t>(body.Body->getUserPointer()))
                    continue;
                mass += EstimateMass(shape.Desc);
                friction += shape.Desc.Material.Friction;
                restitution += shape.Desc.Material.Restitution;
                hasTrigger |= shape.Desc.IsTrigger;
                ++count;
            }
            if (body.Desc.Type != PhysicsBodyType3D::Dynamic)
                mass = 0.0f;
            else if (!body.Desc.AutoMass)
                mass = std::max(body.Desc.Mass, 0.001f);
            else
                mass = std::max(mass, 0.001f);
            btVector3 inertia(0.0f, 0.0f, 0.0f);
            if (mass > 0.0f && body.Compound->getNumChildShapes() > 0)
                body.Compound->calculateLocalInertia(mass, inertia);
            body.Body->setMassProps(mass, inertia);
            body.Body->updateInertiaTensor();
            if (count > 0)
            {
                body.Body->setFriction(std::max(friction / float(count), 0.0f));
                body.Body->setRestitution(std::clamp(restitution / float(count), 0.0f, 1.0f));
            }
            int flags = body.Body->getCollisionFlags();
            if (hasTrigger)
                flags |= btCollisionObject::CF_NO_CONTACT_RESPONSE;
            else
                flags &= ~btCollisionObject::CF_NO_CONTACT_RESPONSE;
            flags |= btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK;
            body.Body->setCollisionFlags(flags);
            m_World->updateSingleAabb(body.Body.get());
        }

        static float EstimateMass(const PhysicsShape3DDesc& desc)
        {
            float volume = 1.0f;
            switch (desc.Type)
            {
            case PhysicsShapeType3D::Box:
                volume = 8.0f * desc.HalfExtents.x * desc.HalfExtents.y * desc.HalfExtents.z;
                break;
            case PhysicsShapeType3D::Sphere:
                volume = 4.0f / 3.0f * glm::pi<float>() * desc.Radius * desc.Radius * desc.Radius;
                break;
            case PhysicsShapeType3D::Capsule:
                volume = glm::pi<float>() * desc.Radius * desc.Radius * std::max(desc.Height - 2.0f * desc.Radius, 0.0f) +
                         4.0f / 3.0f * glm::pi<float>() * desc.Radius * desc.Radius * desc.Radius;
                break;
            default:
                break;
            }
            return std::max(volume * desc.Material.Density, 0.0f);
        }

        static btTransform ConstraintFrame(const glm::vec3& anchor, const glm::vec3& axis, PhysicsConstraintType3D type)
        {
            btTransform frame;
            frame.setIdentity();
            frame.setOrigin(ToBullet(anchor));
            if (glm::dot(axis, axis) > 1.0e-8f)
            {
                const glm::vec3 reference =
                  type == PhysicsConstraintType3D::Hinge || type == PhysicsConstraintType3D::ConeTwist ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
                frame.setRotation(ToBullet(glm::rotation(reference, glm::normalize(axis))));
            }
            return frame;
        }

        bool AcceptObject(const btCollisionObject* object, const PhysicsQueryFilter3D& filter) const
        {
            auto bodyIt = m_BodyLookup.find(static_cast<const btRigidBody*>(object));
            if (bodyIt == m_BodyLookup.end() || (filter.IgnoreBody && bodyIt->second == filter.IgnoreBody))
                return false;
            if (filter.IncludeTriggers)
                return true;
            const BodyRecord* body = FindBody(bodyIt->second);
            return body && (body->Body->getCollisionFlags() & btCollisionObject::CF_NO_CONTACT_RESPONSE) == 0;
        }

        PhysicsShape3DHandle ShapeFrom(const btCollisionObject* object, int childIndex = -1) const
        {
            const auto bodyIt = m_BodyLookup.find(static_cast<const btRigidBody*>(object));
            if (bodyIt == m_BodyLookup.end())
                return {};
            const BodyRecord* body = FindBody(bodyIt->second);
            if (!body)
                return {};
            const btCollisionShape* shape = nullptr;
            if (childIndex >= 0 && childIndex < body->Compound->getNumChildShapes())
                shape = body->Compound->getChildShape(childIndex);
            else if (body->Compound->getNumChildShapes() > 0)
                shape = body->Compound->getChildShape(0);
            return shape ? PhysicsShape3DHandle{ reinterpret_cast<uint64_t>(shape->getUserPointer()) } : PhysicsShape3DHandle{};
        }

        PhysicsQueryHit3D MakeHit(const btCollisionObject* object, const btCollisionWorld::LocalShapeInfo* info, const glm::vec3& point,
                                  const glm::vec3& normal, float fraction, float distance, int childIndex = -1) const
        {
            PhysicsQueryHit3D hit;
            auto bodyIt = m_BodyLookup.find(static_cast<const btRigidBody*>(object));
            if (bodyIt == m_BodyLookup.end())
                return hit;
            hit.Body = bodyIt->second;
            hit.Shape = ShapeFrom(object, info ? info->m_shapePart : childIndex);
            hit.Point = point;
            hit.Normal = normal;
            hit.Fraction = fraction;
            hit.Distance = fraction * distance;
            auto shapeIt = m_Shapes.find(hit.Shape);
            if (shapeIt != m_Shapes.end())
                hit.UserData = shapeIt->second.Desc.UserData;
            return hit;
        }

        void DispatchContacts()
        {
            m_ContactEvents.clear();
            std::unordered_map<BulletContactKey, ContactRecord, BulletContactKeyHash> current;
            const int manifoldCount = m_Dispatcher->getNumManifolds();
            for (int manifoldIndex = 0; manifoldIndex < manifoldCount; ++manifoldIndex)
            {
                btPersistentManifold* manifold = m_Dispatcher->getManifoldByIndexInternal(manifoldIndex);
                const auto* objectA = static_cast<const btCollisionObject*>(manifold->getBody0());
                const auto* objectB = static_cast<const btCollisionObject*>(manifold->getBody1());
                for (int pointIndex = 0; pointIndex < manifold->getNumContacts(); ++pointIndex)
                {
                    const btManifoldPoint& point = manifold->getContactPoint(pointIndex);
                    if (point.getDistance() > 0.0f)
                        continue;
                    const PhysicsShape3DHandle shapeA = ShapeFrom(objectA, point.m_index0);
                    const PhysicsShape3DHandle shapeB = ShapeFrom(objectB, point.m_index1);
                    if (!shapeA || !shapeB)
                        continue;
                    const BulletContactKey key = MakeKey(shapeA, shapeB);
                    ContactRecord& record = current[key];
                    record.Event.ShapeA = shapeA;
                    record.Event.ShapeB = shapeB;
                    record.Event.BodyA = m_BodyLookup.at(static_cast<const btRigidBody*>(objectA));
                    record.Event.BodyB = m_BodyLookup.at(static_cast<const btRigidBody*>(objectB));
                    const auto shapeAIt = m_Shapes.find(shapeA);
                    const auto shapeBIt = m_Shapes.find(shapeB);
                    record.Event.IsTrigger = (shapeAIt != m_Shapes.end() && shapeAIt->second.Desc.IsTrigger) ||
                                             (shapeBIt != m_Shapes.end() && shapeBIt->second.Desc.IsTrigger);
                    if (shapeAIt != m_Shapes.end())
                    {
                        record.Event.ShapeUserDataA = shapeAIt->second.Desc.UserData;
                        record.Event.MaterialA = shapeAIt->second.Desc.Material;
                    }
                    if (shapeBIt != m_Shapes.end())
                    {
                        record.Event.ShapeUserDataB = shapeBIt->second.Desc.UserData;
                        record.Event.MaterialB = shapeBIt->second.Desc.Material;
                    }
                    PhysicsContactPoint3D output;
                    output.Point = FromBullet(point.getPositionWorldOnA());
                    output.Normal = -FromBullet(point.m_normalWorldOnB);
                    output.Separation = point.getDistance();
                    output.NormalImpulse = point.getAppliedImpulse();
                    record.Event.Points.push_back(output);
                }
            }

            for (auto& [key, record] : current)
            {
                record.Event.Type =
                  m_ActiveContacts.find(key) == m_ActiveContacts.end() ? PhysicsContactEventType3D::Enter : PhysicsContactEventType3D::Stay;
                m_ContactEvents.push_back(record.Event);
            }
            for (const auto& [key, record] : m_ActiveContacts)
            {
                if (current.find(key) != current.end())
                    continue;
                PhysicsContactEvent3D event = record.Event;
                event.Type = PhysicsContactEventType3D::Exit;
                event.Points.clear();
                m_ContactEvents.push_back(event);
            }
            m_ActiveContacts = std::move(current);
            NormalizePhysicsContactEvents3D(m_ContactEvents);
            if (m_Callback)
            {
                for (const PhysicsContactEvent3D& event : m_ContactEvents)
                    m_Callback(event);
            }
            m_ContactEvents.clear();
        }

        uint64_t m_NextHandle = 1;
        glm::vec3 m_Gravity{ 0.0f, -9.81f, 0.0f };
        PhysicsContactCallback3D m_Callback;
        Scope<btDefaultCollisionConfiguration> m_CollisionConfiguration;
        Scope<btCollisionDispatcher> m_Dispatcher;
        Scope<btBroadphaseInterface> m_Broadphase;
        Scope<btSequentialImpulseConstraintSolver> m_Solver;
        Scope<btDiscreteDynamicsWorld> m_World;
        UnorderedMap<PhysicsBody3DHandle, BodyRecord> m_Bodies;
        UnorderedMap<PhysicsShape3DHandle, ShapeRecord> m_Shapes;
        UnorderedMap<const btRigidBody*, PhysicsBody3DHandle> m_BodyLookup;
        UnorderedMap<uint64_t, Scope<btTypedConstraint>> m_Constraints;
        std::unordered_map<BulletContactKey, ContactRecord, BulletContactKeyHash> m_ActiveContacts;
        Vector<PhysicsContactEvent3D> m_ContactEvents;
        ContactAddedCallback m_PreviousContactAddedCallback = nullptr;
        static inline BulletPhysicsBackend* s_ActiveBackend = nullptr;
    };

    Scope<Physics3DBackend> CreateBulletPhysicsBackend() { return CreateScope<BulletPhysicsBackend>(); }
} // namespace Crowny

#else

namespace Crowny
{
    Scope<Physics3DBackend> CreateBulletPhysicsBackend() { return nullptr; }
} // namespace Crowny

#endif

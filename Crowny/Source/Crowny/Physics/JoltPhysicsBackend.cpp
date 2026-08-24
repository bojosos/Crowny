#include "cwpch.h"

#include "Crowny/Physics/Physics3D.h"

#if defined(CW_PHYSICS_JOLT)
// Match Jolt's CMake configuration so Release consumers do not emit references to its debug hooks.
#if !defined(CW_DEBUG)
#define JPH_NO_DEBUG
#endif
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/EmptyShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <algorithm>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace Crowny
{
    namespace
    {
        constexpr JPH::ObjectLayer StaticLayer = 0;
        constexpr JPH::ObjectLayer MovingLayer = 1;

        JPH::Vec3 ToJolt(const glm::vec3& value) { return { value.x, value.y, value.z }; }
        JPH::RVec3 ToJoltPosition(const glm::vec3& value) { return { value.x, value.y, value.z }; }
        JPH::Quat ToJolt(const glm::quat& value) { return { value.x, value.y, value.z, value.w }; }
        glm::vec3 FromJolt(JPH::Vec3Arg value) { return { value.GetX(), value.GetY(), value.GetZ() }; }
        glm::vec3 FromJoltPosition(JPH::RVec3Arg value)
        {
            return { static_cast<float>(value.GetX()), static_cast<float>(value.GetY()), static_cast<float>(value.GetZ()) };
        }
        glm::quat FromJolt(JPH::QuatArg value) { return { value.GetW(), value.GetX(), value.GetY(), value.GetZ() }; }

        JPH::EMotionType ToJolt(PhysicsBodyType3D type)
        {
            switch (type)
            {
            case PhysicsBodyType3D::Dynamic:
                return JPH::EMotionType::Dynamic;
            case PhysicsBodyType3D::Kinematic:
                return JPH::EMotionType::Kinematic;
            default:
                return JPH::EMotionType::Static;
            }
        }

        bool ShouldCollide(const PhysicsFilter3D& first, const PhysicsFilter3D& second)
        {
            if (first.Group != 0 && first.Group == second.Group)
                return first.Group > 0;
            const uint32_t firstBit = 1u << std::min(first.Layer, 31u);
            const uint32_t secondBit = 1u << std::min(second.Layer, 31u);
            return (first.Mask & secondBit) != 0 && (second.Mask & firstBit) != 0;
        }

        class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface
        {
        public:
            uint32_t GetNumBroadPhaseLayers() const override { return 2; }
            JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
            {
                return JPH::BroadPhaseLayer(layer == StaticLayer ? 0 : 1);
            }
        };

        class ObjectVsBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
        {
        public:
            bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhase) const override
            {
                return layer != StaticLayer || broadPhase == JPH::BroadPhaseLayer(1);
            }
        };

        class ObjectLayerFilter final : public JPH::ObjectLayerPairFilter
        {
        public:
            bool ShouldCollide(JPH::ObjectLayer first, JPH::ObjectLayer second) const override
            {
                return first != StaticLayer || second != StaticLayer;
            }
        };

        struct JoltRuntimeState
        {
            std::mutex Mutex;
            uint32_t References = 0;
            bool OwnsFactory = false;
        };

        JoltRuntimeState& GetJoltRuntimeState()
        {
            static JoltRuntimeState state;
            return state;
        }

        void AcquireJoltRuntime()
        {
            JoltRuntimeState& state = GetJoltRuntimeState();
            std::lock_guard<std::mutex> lock(state.Mutex);
            if (state.References == 0)
            {
                JPH::RegisterDefaultAllocator();
                if (JPH::Factory::sInstance == nullptr)
                {
                    JPH::Factory::sInstance = new JPH::Factory();
                    JPH::RegisterTypes();
                    state.OwnsFactory = true;
                }
            }
            ++state.References;
        }

        void ReleaseJoltRuntime()
        {
            JoltRuntimeState& state = GetJoltRuntimeState();
            std::lock_guard<std::mutex> lock(state.Mutex);
            if (state.References == 0 || --state.References != 0)
                return;
            if (state.OwnsFactory)
            {
                JPH::UnregisterTypes();
                delete JPH::Factory::sInstance;
                JPH::Factory::sInstance = nullptr;
                state.OwnsFactory = false;
            }
        }
    } // namespace

    class JoltPhysicsBackend final : public Physics3DBackend, private JPH::ContactListener
    {
    public:
        ~JoltPhysicsBackend() override { Shutdown(); }

        Physics3DBackendType GetType() const override { return Physics3DBackendType::Jolt; }
        const char* GetName() const override { return "Jolt 5.6"; }
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
            AcquireJoltRuntime();
            m_RuntimeAcquired = true;

            m_TempAllocator = CreateScope<JPH::TempAllocatorImpl>(32 * 1024 * 1024);
            const uint32_t workerCount = std::max(1u, std::thread::hardware_concurrency()) - 1u;
            m_JobSystem = CreateScope<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, static_cast<int>(workerCount));
            m_Physics = CreateScope<JPH::PhysicsSystem>();
            m_Physics->Init(65536, 0, 65536, 10240, m_BroadPhaseLayers, m_ObjectVsBroadPhase, m_ObjectLayerFilter);
            m_Physics->SetGravity(ToJolt(settings.Gravity));
            m_Physics->SetContactListener(this);
            m_Callback = std::move(callback);
            m_Initialized = true;
            return true;
        }

        void Shutdown() override
        {
            if (m_Initialized)
            {
                m_Physics->SetContactListener(nullptr);
                for (auto& [handle, constraint] : m_Constraints)
                    m_Physics->RemoveConstraint(constraint.GetPtr());
                m_Constraints.clear();
                m_ConstraintRecords.clear();

                JPH::BodyInterface& bodies = m_Physics->GetBodyInterface();
                for (auto& [handle, body] : m_Bodies)
                {
                    bodies.RemoveBody(body.Native);
                    bodies.DestroyBody(body.Native);
                }
                m_Bodies.clear();
                m_NativeBodies.clear();
                m_Shapes.clear();
                m_ActiveContacts.clear();
                m_PendingContacts.clear();
                m_Initialized = false;
            }
            m_Physics.reset();
            m_JobSystem.reset();
            m_TempAllocator.reset();
            m_Callback = nullptr;
            if (m_RuntimeAcquired)
            {
                ReleaseJoltRuntime();
                m_RuntimeAcquired = false;
            }
        }

        void Step(float timestep, uint32_t substeps) override
        {
            if (!m_Initialized || timestep <= 0.0f)
                return;
            m_Physics->Update(timestep, std::max(substeps, 1u), m_TempAllocator.get(), m_JobSystem.get());

            Vector<PhysicsContactEvent3D> pending;
            {
                std::lock_guard<std::mutex> lock(m_ContactMutex);
                pending.swap(m_PendingContacts);
            }
            if (m_Callback)
                for (const PhysicsContactEvent3D& event : pending)
                    m_Callback(event);
        }

        void SetGravity(const glm::vec3& gravity) override
        {
            if (m_Physics)
                m_Physics->SetGravity(ToJolt(gravity));
        }

        PhysicsBody3DHandle CreateBody(const PhysicsBody3DDesc& desc) override
        {
            JPH::EmptyShapeSettings emptySettings(ToJolt(desc.CenterOfMass));
            JPH::ShapeSettings::ShapeResult emptyResult = emptySettings.Create();
            if (emptyResult.HasError())
                return {};

            PhysicsBody3DHandle handle{ m_NextBody++ };
            const JPH::ObjectLayer layer = desc.Type == PhysicsBodyType3D::Static ? StaticLayer : MovingLayer;
            JPH::BodyCreationSettings settings(emptyResult.Get(), ToJoltPosition(desc.Position), ToJolt(desc.Rotation), ToJolt(desc.Type), layer);
            settings.mLinearVelocity = ToJolt(desc.LinearVelocity);
            settings.mAngularVelocity = ToJolt(desc.AngularVelocity);
            settings.mLinearDamping = std::max(desc.LinearDamping, 0.0f);
            settings.mAngularDamping = std::max(desc.AngularDamping, 0.0f);
            settings.mGravityFactor = desc.GravityScale;
            settings.mAllowSleeping = desc.AllowSleep;
            settings.mMotionQuality = desc.Continuous ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
            settings.mUserData = handle.Value;
            settings.mAllowedDOFs = JPH::EAllowedDOFs::All;
            if (desc.LockRotationX)
                settings.mAllowedDOFs &= ~JPH::EAllowedDOFs::RotationX;
            if (desc.LockRotationY)
                settings.mAllowedDOFs &= ~JPH::EAllowedDOFs::RotationY;
            if (desc.LockRotationZ)
                settings.mAllowedDOFs &= ~JPH::EAllowedDOFs::RotationZ;
            if (!desc.AutoMass && desc.Type == PhysicsBodyType3D::Dynamic)
            {
                settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
                settings.mMassPropertiesOverride.mMass = std::max(desc.Mass, 0.0001f);
            }

            JPH::BodyInterface& bodies = m_Physics->GetBodyInterface();
            JPH::Body* body = bodies.CreateBody(settings);
            if (body == nullptr)
                return {};
            const JPH::BodyID id = body->GetID();
            bodies.AddBody(id, desc.StartAwake ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
            BodyRecord record;
            record.Native = id;
            record.Desc = desc;
            m_Bodies.emplace(handle.Value, std::move(record));
            m_NativeBodies.emplace(id.GetIndexAndSequenceNumber(), handle.Value);
            return handle;
        }

        void DestroyBody(PhysicsBody3DHandle body) override
        {
            auto found = m_Bodies.find(body.Value);
            if (found == m_Bodies.end())
                return;
            Vector<uint64_t> constraints;
            for (const auto& [handle, record] : m_ConstraintRecords)
                if (record.BodyA == body || record.BodyB == body)
                    constraints.push_back(handle);
            for (uint64_t handle : constraints)
                DestroyConstraint({ handle });

            JPH::BodyInterface& bodies = m_Physics->GetBodyInterface();
            bodies.RemoveBody(found->second.Native);
            bodies.DestroyBody(found->second.Native);
            m_NativeBodies.erase(found->second.Native.GetIndexAndSequenceNumber());
            for (PhysicsShape3DHandle shape : found->second.Shapes)
                m_Shapes.erase(shape.Value);
            m_Bodies.erase(found);
        }

        PhysicsShape3DHandle AddShape(PhysicsBody3DHandle body, const PhysicsShape3DDesc& desc) override
        {
            auto bodyIt = m_Bodies.find(body.Value);
            if (bodyIt == m_Bodies.end())
                return {};
            PhysicsShape3DHandle handle{ m_NextShape++ };
            JPH::RefConst<JPH::Shape> native = CreateShape(desc, handle);
            if (native == nullptr)
                return {};
            ShapeRecord shape;
            shape.Body = body;
            shape.Desc = desc;
            shape.Native = std::move(native);
            m_Shapes.emplace(handle.Value, std::move(shape));
            bodyIt->second.Shapes.push_back(handle);
            RebuildBodyShape(bodyIt->second);
            return handle;
        }

        void RemoveShape(PhysicsBody3DHandle body, PhysicsShape3DHandle shape) override
        {
            auto bodyIt = m_Bodies.find(body.Value);
            auto shapeIt = m_Shapes.find(shape.Value);
            if (bodyIt == m_Bodies.end() || shapeIt == m_Shapes.end() || shapeIt->second.Body != body)
                return;
            auto& shapes = bodyIt->second.Shapes;
            shapes.erase(std::remove(shapes.begin(), shapes.end(), shape), shapes.end());
            m_Shapes.erase(shapeIt);
            RebuildBodyShape(bodyIt->second);
        }

        void SetBodyTransform(PhysicsBody3DHandle body, const glm::vec3& position, const glm::quat& rotation, bool activate) override
        {
            if (const BodyRecord* record = FindBody(body))
                m_Physics->GetBodyInterface().SetPositionAndRotation(record->Native, ToJoltPosition(position), ToJolt(rotation),
                                                                     activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
        }

        void GetBodyTransform(PhysicsBody3DHandle body, glm::vec3& position, glm::quat& rotation) const override
        {
            if (const BodyRecord* record = FindBody(body))
            {
                JPH::RVec3 nativePosition;
                JPH::Quat nativeRotation;
                m_Physics->GetBodyInterface().GetPositionAndRotation(record->Native, nativePosition, nativeRotation);
                position = FromJoltPosition(nativePosition);
                rotation = FromJolt(nativeRotation);
            }
        }

        void MoveKinematic(PhysicsBody3DHandle body, const glm::vec3& position, const glm::quat& rotation, float timestep) override
        {
            if (const BodyRecord* record = FindBody(body))
                m_Physics->GetBodyInterface().MoveKinematic(record->Native, ToJoltPosition(position), ToJolt(rotation), timestep);
        }

        void SetLinearVelocity(PhysicsBody3DHandle body, const glm::vec3& velocity) override
        {
            if (const BodyRecord* record = FindBody(body))
                m_Physics->GetBodyInterface().SetLinearVelocity(record->Native, ToJolt(velocity));
        }

        glm::vec3 GetLinearVelocity(PhysicsBody3DHandle body) const override
        {
            const BodyRecord* record = FindBody(body);
            return record ? FromJolt(m_Physics->GetBodyInterface().GetLinearVelocity(record->Native)) : glm::vec3(0.0f);
        }

        void SetAngularVelocity(PhysicsBody3DHandle body, const glm::vec3& velocity) override
        {
            if (const BodyRecord* record = FindBody(body))
                m_Physics->GetBodyInterface().SetAngularVelocity(record->Native, ToJolt(velocity));
        }

        glm::vec3 GetAngularVelocity(PhysicsBody3DHandle body) const override
        {
            const BodyRecord* record = FindBody(body);
            return record ? FromJolt(m_Physics->GetBodyInterface().GetAngularVelocity(record->Native)) : glm::vec3(0.0f);
        }

        void AddForce(PhysicsBody3DHandle body, const glm::vec3& force, const glm::vec3* point, PhysicsForceMode3D mode) override
        {
            const BodyRecord* record = FindBody(body);
            if (record == nullptr)
                return;
            JPH::BodyInterface& bodies = m_Physics->GetBodyInterface();
            glm::vec3 value = force;
            if (mode == PhysicsForceMode3D::Acceleration)
                value *= GetBodyMass(*record);
            if (mode == PhysicsForceMode3D::VelocityChange)
                value *= GetBodyMass(*record);
            if (mode == PhysicsForceMode3D::Impulse || mode == PhysicsForceMode3D::VelocityChange)
            {
                if (point)
                    bodies.AddImpulse(record->Native, ToJolt(value), ToJoltPosition(*point));
                else
                    bodies.AddImpulse(record->Native, ToJolt(value));
            }
            else if (point)
                bodies.AddForce(record->Native, ToJolt(value), ToJoltPosition(*point));
            else
                bodies.AddForce(record->Native, ToJolt(value));
        }

        void AddTorque(PhysicsBody3DHandle body, const glm::vec3& torque, PhysicsForceMode3D mode) override
        {
            const BodyRecord* record = FindBody(body);
            if (record == nullptr)
                return;
            if (mode == PhysicsForceMode3D::Impulse || mode == PhysicsForceMode3D::VelocityChange)
                m_Physics->GetBodyInterface().AddAngularImpulse(record->Native, ToJolt(torque));
            else
                m_Physics->GetBodyInterface().AddTorque(record->Native, ToJolt(torque));
        }

        void SetGravityScale(PhysicsBody3DHandle body, float scale) override
        {
            if (const BodyRecord* record = FindBody(body))
                m_Physics->GetBodyInterface().SetGravityFactor(record->Native, scale);
        }

        void SetDamping(PhysicsBody3DHandle body, float linear, float angular) override
        {
            const BodyRecord* record = FindBody(body);
            if (record == nullptr)
                return;
            JPH::BodyLockWrite lock(m_Physics->GetBodyLockInterface(), record->Native);
            if (lock.Succeeded() && lock.GetBody().GetMotionProperties() != nullptr)
            {
                lock.GetBody().GetMotionProperties()->SetLinearDamping(std::max(linear, 0.0f));
                lock.GetBody().GetMotionProperties()->SetAngularDamping(std::max(angular, 0.0f));
            }
        }

        void SetAwake(PhysicsBody3DHandle body, bool awake) override
        {
            if (const BodyRecord* record = FindBody(body))
            {
                if (awake)
                    m_Physics->GetBodyInterface().ActivateBody(record->Native);
                else
                    m_Physics->GetBodyInterface().DeactivateBody(record->Native);
            }
        }

        bool IsAwake(PhysicsBody3DHandle body) const override
        {
            const BodyRecord* record = FindBody(body);
            return record && m_Physics->GetBodyInterface().IsActive(record->Native);
        }

        void SetFilter(PhysicsBody3DHandle body, const PhysicsFilter3D& filter) override
        {
            if (BodyRecord* record = FindBodyMutable(body))
                record->Desc.Filter = filter;
        }

        void SetShapeMaterial(PhysicsShape3DHandle shape, const PhysicsMaterialData& material) override
        {
            auto found = m_Shapes.find(shape.Value);
            if (found == m_Shapes.end())
                return;
            PhysicsShape3DDesc updated = found->second.Desc;
            updated.Material = NormalizePhysicsMaterialData(material);
            JPH::RefConst<JPH::Shape> native = CreateShape(updated, shape);
            if (native == nullptr)
                return;
            found->second.Desc = std::move(updated);
            found->second.Native = std::move(native);
            if (BodyRecord* body = FindBodyMutable(found->second.Body))
                RebuildBodyShape(*body);
        }

        void SetShapeTrigger(PhysicsShape3DHandle shape, bool trigger) override
        {
            auto found = m_Shapes.find(shape.Value);
            if (found != m_Shapes.end())
                found->second.Desc.IsTrigger = trigger;
        }

        PhysicsConstraint3DHandle CreateConstraint(const PhysicsConstraint3DDesc& desc) override
        {
            const BodyRecord* first = FindBody(desc.BodyA);
            const BodyRecord* second = FindBody(desc.BodyB);
            if (first == nullptr || second == nullptr)
                return {};
            JPH::BodyID ids[] = { first->Native, second->Native };
            JPH::BodyLockMultiWrite lock(m_Physics->GetBodyLockInterface(), ids, 2);
            JPH::Body* bodyA = lock.GetBody(0);
            JPH::Body* bodyB = lock.GetBody(1);
            if (bodyA == nullptr || bodyB == nullptr)
                return {};

            JPH::Ref<JPH::Constraint> constraint;
            const JPH::Vec3 axisA = SafeAxis(desc.AxisA);
            const JPH::Vec3 axisB = SafeAxis(desc.AxisB);
            const JPH::Vec3 normalA = Perpendicular(axisA);
            const JPH::Vec3 normalB = Perpendicular(axisB);
            switch (desc.Type)
            {
            case PhysicsConstraintType3D::Fixed: {
                JPH::FixedConstraintSettings settings;
                settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
                settings.mPoint1 = ToJoltPosition(desc.AnchorA);
                settings.mPoint2 = ToJoltPosition(desc.AnchorB);
                settings.mAxisX1 = axisA;
                settings.mAxisX2 = axisB;
                settings.mAxisY1 = normalA;
                settings.mAxisY2 = normalB;
                constraint = settings.Create(*bodyA, *bodyB);
                break;
            }
            case PhysicsConstraintType3D::Point: {
                JPH::PointConstraintSettings settings;
                settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
                settings.mPoint1 = ToJoltPosition(desc.AnchorA);
                settings.mPoint2 = ToJoltPosition(desc.AnchorB);
                constraint = settings.Create(*bodyA, *bodyB);
                break;
            }
            case PhysicsConstraintType3D::Distance:
            case PhysicsConstraintType3D::Spring: {
                JPH::DistanceConstraintSettings settings;
                settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
                settings.mPoint1 = ToJoltPosition(desc.AnchorA);
                settings.mPoint2 = ToJoltPosition(desc.AnchorB);
                if (desc.EnableLimits)
                {
                    settings.mMinDistance = std::max(desc.MinimumLimit, 0.0f);
                    settings.mMaxDistance = std::max(desc.MaximumLimit, settings.mMinDistance);
                }
                if (desc.EnableSpring || desc.Type == PhysicsConstraintType3D::Spring)
                    ConfigureSpring(settings.mLimitsSpringSettings, desc);
                constraint = settings.Create(*bodyA, *bodyB);
                break;
            }
            case PhysicsConstraintType3D::Hinge: {
                JPH::HingeConstraintSettings settings;
                settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
                settings.mPoint1 = ToJoltPosition(desc.AnchorA);
                settings.mPoint2 = ToJoltPosition(desc.AnchorB);
                settings.mHingeAxis1 = axisA;
                settings.mHingeAxis2 = axisB;
                settings.mNormalAxis1 = normalA;
                settings.mNormalAxis2 = normalB;
                if (desc.EnableLimits)
                {
                    settings.mLimitsMin = desc.MinimumLimit;
                    settings.mLimitsMax = desc.MaximumLimit;
                }
                ConfigureMotor(settings.mMotorSettings, desc);
                JPH::HingeConstraint* hinge = static_cast<JPH::HingeConstraint*>(settings.Create(*bodyA, *bodyB));
                if (desc.EnableMotor)
                {
                    hinge->SetMotorState(JPH::EMotorState::Velocity);
                    hinge->SetTargetAngularVelocity(desc.MotorTargetVelocity);
                }
                constraint = hinge;
                break;
            }
            case PhysicsConstraintType3D::Slider: {
                JPH::SliderConstraintSettings settings;
                settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
                settings.mPoint1 = ToJoltPosition(desc.AnchorA);
                settings.mPoint2 = ToJoltPosition(desc.AnchorB);
                settings.mSliderAxis1 = axisA;
                settings.mSliderAxis2 = axisB;
                settings.mNormalAxis1 = normalA;
                settings.mNormalAxis2 = normalB;
                if (desc.EnableLimits)
                {
                    settings.mLimitsMin = desc.MinimumLimit;
                    settings.mLimitsMax = desc.MaximumLimit;
                }
                ConfigureMotor(settings.mMotorSettings, desc);
                JPH::SliderConstraint* slider = static_cast<JPH::SliderConstraint*>(settings.Create(*bodyA, *bodyB));
                if (desc.EnableMotor)
                {
                    slider->SetMotorState(JPH::EMotorState::Velocity);
                    slider->SetTargetVelocity(desc.MotorTargetVelocity);
                }
                constraint = slider;
                break;
            }
            case PhysicsConstraintType3D::ConeTwist: {
                JPH::SwingTwistConstraintSettings settings;
                settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
                settings.mPosition1 = ToJoltPosition(desc.AnchorA);
                settings.mPosition2 = ToJoltPosition(desc.AnchorB);
                settings.mTwistAxis1 = axisA;
                settings.mTwistAxis2 = axisB;
                settings.mPlaneAxis1 = normalA;
                settings.mPlaneAxis2 = normalB;
                if (desc.EnableLimits)
                {
                    settings.mNormalHalfConeAngle = std::max(std::abs(desc.MaximumLimit), 0.0f);
                    settings.mPlaneHalfConeAngle = settings.mNormalHalfConeAngle;
                    settings.mTwistMinAngle = desc.MinimumLimit;
                    settings.mTwistMaxAngle = desc.MaximumLimit;
                }
                constraint = settings.Create(*bodyA, *bodyB);
                break;
            }
            }
            if (constraint == nullptr)
                return {};
            lock.ReleaseLocks();
            m_Physics->AddConstraint(constraint.GetPtr());
            PhysicsConstraint3DHandle handle{ m_NextConstraint++ };
            m_Constraints.emplace(handle.Value, std::move(constraint));
            m_ConstraintRecords.emplace(handle.Value, desc);
            return handle;
        }

        void DestroyConstraint(PhysicsConstraint3DHandle constraint) override
        {
            auto found = m_Constraints.find(constraint.Value);
            if (found == m_Constraints.end())
                return;
            m_Physics->RemoveConstraint(found->second.GetPtr());
            m_Constraints.erase(found);
            m_ConstraintRecords.erase(constraint.Value);
        }

        Vector<PhysicsQueryHit3D> Raycast(const glm::vec3& origin, const glm::vec3& direction, float distance,
                                          const PhysicsQueryFilter3D& filter) const override
        {
            Vector<PhysicsQueryHit3D> result;
            const glm::vec3 rayDirection = glm::normalize(direction) * distance;
            JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
            m_Physics->GetNarrowPhaseQuery().CastRay(JPH::RRayCast(ToJoltPosition(origin), ToJolt(rayDirection)), JPH::RayCastSettings(), collector);
            collector.Sort();
            for (const JPH::RayCastResult& hit : collector.mHits)
            {
                PhysicsQueryHit3D converted;
                if (!ConvertQueryHit(hit.mBodyID, hit.mSubShapeID2, filter, converted))
                    continue;
                converted.Fraction = hit.mFraction;
                converted.Distance = hit.mFraction * distance;
                converted.Point = origin + rayDirection * hit.mFraction;
                JPH::BodyLockRead lock(m_Physics->GetBodyLockInterface(), hit.mBodyID);
                if (lock.Succeeded())
                    converted.Normal = FromJolt(lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, ToJoltPosition(converted.Point)));
                result.push_back(converted);
            }
            return result;
        }

        Vector<PhysicsQueryHit3D> Sweep(const PhysicsShape3DDesc& shape, const glm::vec3& position, const glm::quat& rotation,
                                        const glm::vec3& direction, float distance, const PhysicsQueryFilter3D& filter) const override
        {
            Vector<PhysicsQueryHit3D> result;
            JPH::RefConst<JPH::Shape> native = CreateShape(shape, {});
            if (native == nullptr || native->GetType() != JPH::EShapeType::Convex)
                return result;
            const glm::vec3 castDirection = glm::normalize(direction) * distance;
            const glm::vec3 start = position + rotation * shape.LocalPosition;
            const glm::quat orientation = rotation * shape.LocalRotation;
            const JPH::RMat44 transform = JPH::RMat44::sRotationTranslation(ToJolt(orientation), ToJoltPosition(start));
            JPH::AllHitCollisionCollector<JPH::CastShapeCollector> collector;
            const JPH::RShapeCast cast =
              JPH::RShapeCast::sFromWorldTransform(native.GetPtr(), JPH::Vec3::sReplicate(1.0f), transform, ToJolt(castDirection));
            m_Physics->GetNarrowPhaseQuery().CastShape(cast, JPH::ShapeCastSettings(), JPH::RVec3::sZero(), collector);
            collector.Sort();
            for (const JPH::ShapeCastResult& hit : collector.mHits)
            {
                PhysicsQueryHit3D converted;
                if (!ConvertQueryHit(hit.mBodyID2, hit.mSubShapeID2, filter, converted))
                    continue;
                converted.Fraction = hit.mFraction;
                converted.Distance = hit.mFraction * distance;
                converted.Point = FromJolt(hit.mContactPointOn2);
                converted.Normal = FromJolt(-hit.mPenetrationAxis.NormalizedOr(JPH::Vec3::sAxisY()));
                result.push_back(converted);
            }
            return result;
        }

        Vector<PhysicsQueryHit3D> Overlap(const PhysicsShape3DDesc& shape, const glm::vec3& position, const glm::quat& rotation,
                                          const PhysicsQueryFilter3D& filter) const override
        {
            Vector<PhysicsQueryHit3D> result;
            JPH::RefConst<JPH::Shape> native = CreateShape(shape, {});
            if (native == nullptr)
                return result;
            const glm::vec3 center = position + rotation * shape.LocalPosition;
            const glm::quat orientation = rotation * shape.LocalRotation;
            const JPH::RMat44 transform = JPH::RMat44::sRotationTranslation(ToJolt(orientation), ToJoltPosition(center));
            JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
            m_Physics->GetNarrowPhaseQuery().CollideShape(native.GetPtr(), JPH::Vec3::sReplicate(1.0f), transform, JPH::CollideShapeSettings(),
                                                          JPH::RVec3::sZero(), collector);
            for (const JPH::CollideShapeResult& hit : collector.mHits)
            {
                PhysicsQueryHit3D converted;
                if (!ConvertQueryHit(hit.mBodyID2, hit.mSubShapeID2, filter, converted))
                    continue;
                converted.Point = FromJolt(hit.mContactPointOn2);
                converted.Normal = FromJolt(-hit.mPenetrationAxis.NormalizedOr(JPH::Vec3::sAxisY()));
                converted.Distance = -hit.mPenetrationDepth;
                result.push_back(converted);
            }
            return result;
        }

    private:
        struct BodyRecord
        {
            JPH::BodyID Native;
            PhysicsBody3DDesc Desc;
            Vector<PhysicsShape3DHandle> Shapes;
        };

        struct ShapeRecord
        {
            PhysicsBody3DHandle Body;
            PhysicsShape3DDesc Desc;
            JPH::RefConst<JPH::Shape> Native;
        };

        const BodyRecord* FindBody(PhysicsBody3DHandle handle) const
        {
            auto found = m_Bodies.find(handle.Value);
            return found == m_Bodies.end() ? nullptr : &found->second;
        }

        BodyRecord* FindBodyMutable(PhysicsBody3DHandle handle)
        {
            auto found = m_Bodies.find(handle.Value);
            return found == m_Bodies.end() ? nullptr : &found->second;
        }

        static JPH::Vec3 SafeAxis(const glm::vec3& axis)
        {
            const JPH::Vec3 converted = ToJolt(axis);
            return converted.LengthSq() > 1.0e-8f ? converted.Normalized() : JPH::Vec3::sAxisX();
        }

        static JPH::Vec3 Perpendicular(JPH::Vec3Arg axis)
        {
            const JPH::Vec3 reference = std::abs(axis.GetY()) < 0.9f ? JPH::Vec3::sAxisY() : JPH::Vec3::sAxisZ();
            return axis.Cross(reference).NormalizedOr(JPH::Vec3::sAxisZ());
        }

        static void ConfigureSpring(JPH::SpringSettings& spring, const PhysicsConstraint3DDesc& desc)
        {
            spring.mMode = JPH::ESpringMode::FrequencyAndDamping;
            spring.mFrequency = std::max(desc.Frequency, 0.0f);
            spring.mDamping = std::max(desc.Damping, 0.0f);
        }

        static void ConfigureMotor(JPH::MotorSettings& motor, const PhysicsConstraint3DDesc& desc)
        {
            if (!desc.EnableMotor)
                return;
            motor.mMinForceLimit = -std::max(desc.MaximumMotorForce, 0.0f);
            motor.mMaxForceLimit = std::max(desc.MaximumMotorForce, 0.0f);
            motor.mMinTorqueLimit = motor.mMinForceLimit;
            motor.mMaxTorqueLimit = motor.mMaxForceLimit;
        }

        JPH::RefConst<JPH::Shape> CreateShape(const PhysicsShape3DDesc& desc, PhysicsShape3DHandle handle) const
        {
            JPH::ShapeSettings::ShapeResult result;
            switch (desc.Type)
            {
            case PhysicsShapeType3D::Box: {
                JPH::BoxShapeSettings settings(ToJolt(glm::max(glm::abs(desc.HalfExtents), glm::vec3(0.001f))));
                settings.mDensity = std::max(desc.Material.Density, 0.0001f);
                settings.mUserData = handle.Value;
                result = settings.Create();
                break;
            }
            case PhysicsShapeType3D::Sphere: {
                JPH::SphereShapeSettings settings(std::max(desc.Radius, 0.001f));
                settings.mDensity = std::max(desc.Material.Density, 0.0001f);
                settings.mUserData = handle.Value;
                result = settings.Create();
                break;
            }
            case PhysicsShapeType3D::Capsule: {
                const float radius = std::max(desc.Radius, 0.001f);
                JPH::CapsuleShapeSettings settings(std::max(desc.Height * 0.5f - radius, 0.001f), radius);
                settings.mDensity = std::max(desc.Material.Density, 0.0001f);
                settings.mUserData = handle.Value;
                result = settings.Create();
                break;
            }
            case PhysicsShapeType3D::ConvexHull: {
                JPH::Array<JPH::Vec3> vertices;
                vertices.reserve(desc.Vertices.size());
                for (const glm::vec3& vertex : desc.Vertices)
                    vertices.push_back(ToJolt(vertex));
                JPH::ConvexHullShapeSettings settings(std::move(vertices));
                settings.mDensity = std::max(desc.Material.Density, 0.0001f);
                settings.mUserData = handle.Value;
                result = settings.Create();
                break;
            }
            case PhysicsShapeType3D::TriangleMesh: {
                result = CreateMesh(desc.Vertices, desc.Indices, handle);
                break;
            }
            case PhysicsShapeType3D::HeightField: {
                Vector<glm::vec3> vertices;
                Vector<uint32_t> indices;
                BuildHeightFieldMesh(desc, vertices, indices);
                result = CreateMesh(vertices, indices, handle);
                break;
            }
            }
            if (result.HasError())
            {
                CW_ENGINE_ERROR("Jolt failed to create a physics shape: {0}", result.GetError().c_str());
                return nullptr;
            }
            return result.Get();
        }

        static JPH::ShapeSettings::ShapeResult CreateMesh(const Vector<glm::vec3>& sourceVertices, const Vector<uint32_t>& sourceIndices,
                                                          PhysicsShape3DHandle handle)
        {
            JPH::VertexList vertices;
            vertices.reserve(sourceVertices.size());
            for (const glm::vec3& vertex : sourceVertices)
                vertices.emplace_back(vertex.x, vertex.y, vertex.z);
            JPH::IndexedTriangleList triangles;
            triangles.reserve(sourceIndices.size() / 3);
            for (size_t i = 0; i + 2 < sourceIndices.size(); i += 3)
                triangles.emplace_back(sourceIndices[i], sourceIndices[i + 1], sourceIndices[i + 2]);
            JPH::MeshShapeSettings settings(std::move(vertices), std::move(triangles));
            settings.mUserData = handle.Value;
            return settings.Create();
        }

        static void BuildHeightFieldMesh(const PhysicsShape3DDesc& desc, Vector<glm::vec3>& vertices, Vector<uint32_t>& indices)
        {
            if (desc.HeightFieldRows < 2 || desc.HeightFieldColumns < 2 ||
                desc.Heights.size() < static_cast<size_t>(desc.HeightFieldRows) * desc.HeightFieldColumns)
                return;
            vertices.reserve(desc.Heights.size());
            for (uint32_t row = 0; row < desc.HeightFieldRows; ++row)
                for (uint32_t column = 0; column < desc.HeightFieldColumns; ++column)
                    vertices.emplace_back(column * desc.HeightFieldScale.x,
                                          desc.Heights[row * desc.HeightFieldColumns + column] * desc.HeightFieldScale.y,
                                          row * desc.HeightFieldScale.z);
            for (uint32_t row = 0; row + 1 < desc.HeightFieldRows; ++row)
                for (uint32_t column = 0; column + 1 < desc.HeightFieldColumns; ++column)
                {
                    const uint32_t a = row * desc.HeightFieldColumns + column;
                    const uint32_t b = a + 1;
                    const uint32_t c = a + desc.HeightFieldColumns;
                    const uint32_t d = c + 1;
                    indices.insert(indices.end(), { a, c, b, b, c, d });
                }
        }

        void RebuildBodyShape(BodyRecord& body)
        {
            JPH::RefConst<JPH::Shape> root;
            if (body.Shapes.empty())
            {
                JPH::EmptyShapeSettings settings(ToJolt(body.Desc.CenterOfMass));
                root = settings.Create().Get();
            }
            else if (body.Shapes.size() == 1)
            {
                const ShapeRecord& shape = m_Shapes.at(body.Shapes.front().Value);
                if (glm::dot(shape.Desc.LocalPosition, shape.Desc.LocalPosition) <= 1.0e-12f &&
                    std::abs(shape.Desc.LocalRotation.w - 1.0f) <= 1.0e-6f &&
                    glm::dot(glm::vec3(shape.Desc.LocalRotation.x, shape.Desc.LocalRotation.y, shape.Desc.LocalRotation.z),
                             glm::vec3(shape.Desc.LocalRotation.x, shape.Desc.LocalRotation.y, shape.Desc.LocalRotation.z)) <= 1.0e-12f)
                    root = shape.Native;
            }
            if (root == nullptr)
            {
                JPH::StaticCompoundShapeSettings compound;
                for (PhysicsShape3DHandle handle : body.Shapes)
                {
                    const ShapeRecord& shape = m_Shapes.at(handle.Value);
                    compound.AddShape(ToJolt(shape.Desc.LocalPosition), ToJolt(shape.Desc.LocalRotation), shape.Native.GetPtr(),
                                      static_cast<uint32_t>(handle.Value));
                }
                JPH::ShapeSettings::ShapeResult result = compound.Create();
                if (result.HasError())
                {
                    CW_ENGINE_ERROR("Jolt failed to rebuild compound shape: {0}", result.GetError().c_str());
                    return;
                }
                root = result.Get();
            }
            const JPH::EActivation activation = body.Desc.StartAwake ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;
            m_Physics->GetBodyInterface().SetShape(body.Native, root.GetPtr(), true, activation);
            if (!body.Desc.AutoMass && body.Desc.Type == PhysicsBodyType3D::Dynamic)
            {
                JPH::BodyLockWrite lock(m_Physics->GetBodyLockInterface(), body.Native);
                if (lock.Succeeded() && lock.GetBody().GetMotionProperties() != nullptr)
                    lock.GetBody().GetMotionProperties()->ScaleToMass(std::max(body.Desc.Mass, 0.0001f));
            }
            UpdateBodyMaterial(body);
        }

        void UpdateBodyMaterial(const BodyRecord& body)
        {
            if (body.Shapes.empty())
                return;
            float friction = 0.0f;
            float restitution = 0.0f;
            for (PhysicsShape3DHandle handle : body.Shapes)
            {
                const PhysicsMaterialData& material = m_Shapes.at(handle.Value).Desc.Material;
                friction += material.Friction;
                restitution += material.Restitution;
            }
            const float inverseCount = 1.0f / static_cast<float>(body.Shapes.size());
            m_Physics->GetBodyInterface().SetFriction(body.Native, std::max(friction * inverseCount, 0.0f));
            m_Physics->GetBodyInterface().SetRestitution(body.Native, glm::clamp(restitution * inverseCount, 0.0f, 1.0f));
        }

        float GetBodyMass(const BodyRecord& body) const
        {
            JPH::BodyLockRead lock(m_Physics->GetBodyLockInterface(), body.Native);
            if (!lock.Succeeded() || lock.GetBody().GetMotionProperties() == nullptr)
                return 0.0f;
            const float inverseMass = lock.GetBody().GetMotionProperties()->GetInverseMass();
            return inverseMass > 0.0f ? 1.0f / inverseMass : 0.0f;
        }

        PhysicsShape3DHandle ResolveShape(const JPH::Body& body, const JPH::SubShapeID& subShape) const
        {
            PhysicsShape3DHandle handle{ body.GetShape()->GetSubShapeUserData(subShape) };
            if (m_Shapes.find(handle.Value) != m_Shapes.end())
                return handle;
            const BodyRecord* record = FindBody({ body.GetUserData() });
            return record && !record->Shapes.empty() ? record->Shapes.front() : PhysicsShape3DHandle{};
        }

        bool ConvertQueryHit(const JPH::BodyID& nativeBody, const JPH::SubShapeID& subShape, const PhysicsQueryFilter3D& filter,
                             PhysicsQueryHit3D& result) const
        {
            auto native = m_NativeBodies.find(nativeBody.GetIndexAndSequenceNumber());
            if (native == m_NativeBodies.end())
                return false;
            result.Body = { native->second };
            if (result.Body == filter.IgnoreBody)
                return false;
            const BodyRecord* body = FindBody(result.Body);
            if (body == nullptr || (filter.LayerMask & (1u << std::min(body->Desc.Filter.Layer, 31u))) == 0)
                return false;
            JPH::BodyLockRead lock(m_Physics->GetBodyLockInterface(), nativeBody);
            if (!lock.Succeeded())
                return false;
            result.Shape = ResolveShape(lock.GetBody(), subShape);
            auto shape = m_Shapes.find(result.Shape.Value);
            if (shape == m_Shapes.end() || (!filter.IncludeTriggers && shape->second.Desc.IsTrigger))
                return false;
            result.UserData = shape->second.Desc.UserData != 0 ? shape->second.Desc.UserData : body->Desc.UserData;
            return true;
        }

        JPH::ValidateResult OnContactValidate(const JPH::Body& bodyA, const JPH::Body& bodyB, JPH::RVec3Arg,
                                              const JPH::CollideShapeResult& collision) override
        {
            const BodyRecord* firstBody = FindBody({ bodyA.GetUserData() });
            const BodyRecord* secondBody = FindBody({ bodyB.GetUserData() });
            if (firstBody == nullptr || secondBody == nullptr || !ShouldCollide(firstBody->Desc.Filter, secondBody->Desc.Filter))
                return JPH::ValidateResult::RejectContact;
            const PhysicsShape3DHandle firstShape = ResolveShape(bodyA, collision.mSubShapeID1);
            const PhysicsShape3DHandle secondShape = ResolveShape(bodyB, collision.mSubShapeID2);
            const auto first = m_Shapes.find(firstShape.Value);
            const auto second = m_Shapes.find(secondShape.Value);
            if (first == m_Shapes.end() || second == m_Shapes.end() || !ShouldCollide(first->second.Desc.Filter, second->second.Desc.Filter))
                return JPH::ValidateResult::RejectContact;
            return JPH::ValidateResult::AcceptContact;
        }

        void OnContactAdded(const JPH::Body& bodyA, const JPH::Body& bodyB, const JPH::ContactManifold& manifold,
                            JPH::ContactSettings& settings) override
        {
            QueueContact(PhysicsContactEventType3D::Enter, bodyA, bodyB, manifold, settings);
        }

        void OnContactPersisted(const JPH::Body& bodyA, const JPH::Body& bodyB, const JPH::ContactManifold& manifold,
                                JPH::ContactSettings& settings) override
        {
            QueueContact(PhysicsContactEventType3D::Stay, bodyA, bodyB, manifold, settings);
        }

        void OnContactRemoved(const JPH::SubShapeIDPair& pair) override
        {
            std::lock_guard<std::mutex> lock(m_ContactMutex);
            auto found = m_ActiveContacts.find(pair);
            if (found == m_ActiveContacts.end())
                return;
            PhysicsContactEvent3D event = found->second;
            event.Type = PhysicsContactEventType3D::Exit;
            event.Points.clear();
            m_PendingContacts.push_back(std::move(event));
            m_ActiveContacts.erase(found);
        }

        void QueueContact(PhysicsContactEventType3D type, const JPH::Body& bodyA, const JPH::Body& bodyB, const JPH::ContactManifold& manifold,
                          JPH::ContactSettings& settings)
        {
            PhysicsContactEvent3D event;
            event.Type = type;
            event.BodyA = { bodyA.GetUserData() };
            event.BodyB = { bodyB.GetUserData() };
            event.ShapeA = ResolveShape(bodyA, manifold.mSubShapeID1);
            event.ShapeB = ResolveShape(bodyB, manifold.mSubShapeID2);
            const auto shapeA = m_Shapes.find(event.ShapeA.Value);
            const auto shapeB = m_Shapes.find(event.ShapeB.Value);
            if (shapeA == m_Shapes.end() || shapeB == m_Shapes.end())
                return;
            event.IsTrigger = shapeA->second.Desc.IsTrigger || shapeB->second.Desc.IsTrigger;
            settings.mIsSensor = event.IsTrigger;
            settings.mCombinedFriction = CombinePhysicsMaterialValue(
              shapeA->second.Desc.Material.Friction, shapeA->second.Desc.Material.FrictionCombine, shapeB->second.Desc.Material.Friction,
              shapeB->second.Desc.Material.FrictionCombine);
            settings.mCombinedRestitution = CombinePhysicsMaterialValue(
              shapeA->second.Desc.Material.Restitution, shapeA->second.Desc.Material.RestitutionCombine,
              shapeB->second.Desc.Material.Restitution, shapeB->second.Desc.Material.RestitutionCombine);
            event.Points.reserve(manifold.mRelativeContactPointsOn1.size());
            for (uint32_t i = 0; i < manifold.mRelativeContactPointsOn1.size(); ++i)
            {
                PhysicsContactPoint3D point;
                point.Point = FromJoltPosition(manifold.GetWorldSpaceContactPointOn1(i));
                point.Normal = FromJolt(manifold.mWorldSpaceNormal);
                point.Separation = -manifold.mPenetrationDepth;
                event.Points.push_back(point);
            }
            const JPH::SubShapeIDPair key(bodyA.GetID(), manifold.mSubShapeID1, bodyB.GetID(), manifold.mSubShapeID2);
            std::lock_guard<std::mutex> lock(m_ContactMutex);
            m_ActiveContacts[key] = event;
            m_PendingContacts.push_back(std::move(event));
        }

        BroadPhaseLayers m_BroadPhaseLayers;
        ObjectVsBroadPhaseFilter m_ObjectVsBroadPhase;
        ObjectLayerFilter m_ObjectLayerFilter;
        Scope<JPH::PhysicsSystem> m_Physics;
        Scope<JPH::TempAllocatorImpl> m_TempAllocator;
        Scope<JPH::JobSystemThreadPool> m_JobSystem;
        std::unordered_map<uint64_t, BodyRecord> m_Bodies;
        std::unordered_map<uint32_t, uint64_t> m_NativeBodies;
        std::unordered_map<uint64_t, ShapeRecord> m_Shapes;
        std::unordered_map<uint64_t, JPH::Ref<JPH::Constraint>> m_Constraints;
        std::unordered_map<uint64_t, PhysicsConstraint3DDesc> m_ConstraintRecords;
        std::unordered_map<JPH::SubShapeIDPair, PhysicsContactEvent3D> m_ActiveContacts;
        Vector<PhysicsContactEvent3D> m_PendingContacts;
        PhysicsContactCallback3D m_Callback;
        std::mutex m_ContactMutex;
        uint64_t m_NextBody = 1;
        uint64_t m_NextShape = 1;
        uint64_t m_NextConstraint = 1;
        bool m_Initialized = false;
        bool m_RuntimeAcquired = false;
    };

    Scope<Physics3DBackend> CreateJoltPhysicsBackend() { return CreateScope<JoltPhysicsBackend>(); }
} // namespace Crowny

#else
namespace Crowny
{
    Scope<Physics3DBackend> CreateJoltPhysicsBackend() { return nullptr; }
} // namespace Crowny
#endif

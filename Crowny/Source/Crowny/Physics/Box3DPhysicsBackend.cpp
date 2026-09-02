#include "cwpch.h"

#include "Crowny/Physics/Physics3D.h"

#if defined(CW_PHYSICS_BOX3D)
#include <box3d/box3d.h>

#include <algorithm>
#include <cmath>

#include <glm/gtx/quaternion.hpp>

namespace Crowny
{
    namespace
    {
        b3Vec3 ToBox(const glm::vec3& value) { return { value.x, value.y, value.z }; }
        b3Pos ToBoxPosition(const glm::vec3& value) { return { value.x, value.y, value.z }; }
        b3Quat ToBox(const glm::quat& value) { return { { value.x, value.y, value.z }, value.w }; }
        glm::vec3 FromBox(b3Vec3 value) { return { value.x, value.y, value.z }; }
        glm::vec3 FromBoxPosition(b3Pos value) { return { static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z) }; }
        glm::quat FromBox(b3Quat value) { return { value.s, value.v.x, value.v.y, value.v.z }; }

        b3Filter ToBox(const PhysicsFilter3D& filter)
        {
            b3Filter result = b3DefaultFilter();
            result.categoryBits = uint64_t(1) << std::min(filter.Layer, 63u);
            result.maskBits = filter.Mask;
            result.groupIndex = filter.Group;
            return result;
        }

        b3QueryFilter ToBox(const PhysicsQueryFilter3D& filter)
        {
            b3QueryFilter result = b3DefaultQueryFilter();
            result.categoryBits = UINT64_MAX;
            result.maskBits = filter.LayerMask;
            return result;
        }

        b3Transform MakeFrame(const glm::vec3& anchor, const glm::vec3& axis, const glm::vec3& referenceAxis)
        {
            glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
            if (glm::dot(axis, axis) > 1.0e-8f)
                rotation = glm::rotation(referenceAxis, glm::normalize(axis));
            return { ToBox(anchor), ToBox(rotation) };
        }

        struct ContactPair
        {
            uint64_t A = 0;
            uint64_t B = 0;

            bool operator==(const ContactPair& other) const { return A == other.A && B == other.B; }
        };

        struct ContactPairHash
        {
            size_t operator()(const ContactPair& value) const
            {
                const size_t first = std::hash<uint64_t>{}(value.A);
                const size_t second = std::hash<uint64_t>{}(value.B);
                return first ^ (second + 0x9e3779b97f4a7c15ull + (first << 6) + (first >> 2));
            }
        };

        ContactPair MakePair(uint64_t first, uint64_t second) { return first < second ? ContactPair{ first, second } : ContactPair{ second, first }; }

        uint64_t PackMaterialModes(const PhysicsMaterialData& material)
        {
            return static_cast<uint64_t>(material.FrictionCombine) | (static_cast<uint64_t>(material.RestitutionCombine) << 8u);
        }

        float CombineFriction(float first, uint64_t firstId, float second, uint64_t secondId)
        {
            return CombinePhysicsMaterialValue(first, static_cast<PhysicsCombineMode>(firstId & 0xFFu), second,
                                               static_cast<PhysicsCombineMode>(secondId & 0xFFu));
        }

        float CombineRestitution(float first, uint64_t firstId, float second, uint64_t secondId)
        {
            return CombinePhysicsMaterialValue(first, static_cast<PhysicsCombineMode>((firstId >> 8u) & 0xFFu), second,
                                               static_cast<PhysicsCombineMode>((secondId >> 8u) & 0xFFu));
        }
    } // namespace

    class Box3DPhysicsBackend final : public Physics3DBackend
    {
    public:
        ~Box3DPhysicsBackend() override { Shutdown(); }

        Physics3DBackendType GetType() const override { return Physics3DBackendType::Box3D; }
        const char* GetName() const override { return "Box3D 0.1"; }
        Physics3DCapability GetCapabilities() const override
        {
            return Physics3DCapability::RigidBodies | Physics3DCapability::PrimitiveShapes | Physics3DCapability::ConvexShapes |
                   Physics3DCapability::TriangleMeshes | Physics3DCapability::HeightFields | Physics3DCapability::Sensors |
                   Physics3DCapability::ContinuousCollision | Physics3DCapability::RayCasts | Physics3DCapability::ShapeCasts |
                   Physics3DCapability::Overlaps | Physics3DCapability::Constraints | Physics3DCapability::Motors | Physics3DCapability::Springs |
                   Physics3DCapability::DeterministicSimulation;
        }

        bool Initialize(const Physics3DSettings& settings, PhysicsContactCallback3D callback) override
        {
            Shutdown();
            b3WorldDef worldDef = b3DefaultWorldDef();
            worldDef.gravity = ToBox(settings.Gravity);
            worldDef.frictionCallback = &CombineFriction;
            worldDef.restitutionCallback = &CombineRestitution;
            m_World = b3CreateWorld(&worldDef);
            m_Callback = std::move(callback);
            return B3_IS_NON_NULL(m_World);
        }

        void Shutdown() override
        {
            if (B3_IS_NON_NULL(m_World))
                b3DestroyWorld(m_World);
            m_World = b3_nullWorldId;
            m_Bodies.clear();
            for (auto& [handle, shape] : m_Shapes)
                DestroyShapeResource(shape);
            m_Shapes.clear();
            m_NativeShapes.clear();
            m_Constraints.clear();
            m_ActiveContacts.clear();
            m_ContactEvents.clear();
            m_Callback = nullptr;
        }

        void Step(float timestep, uint32_t substeps) override
        {
            if (B3_IS_NULL(m_World) || timestep <= 0.0f)
                return;
            b3World_Step(m_World, timestep, static_cast<int>(std::max(substeps, 1u)));
            SnapshotEvents();
        }

        void SetGravity(const glm::vec3& gravity) override
        {
            if (B3_IS_NON_NULL(m_World))
                b3World_SetGravity(m_World, ToBox(gravity));
        }

        PhysicsBody3DHandle CreateBody(const PhysicsBody3DDesc& desc) override
        {
            if (B3_IS_NULL(m_World))
                return {};
            const PhysicsBody3DHandle handle{ NextHandle() };
            b3BodyDef bodyDef = b3DefaultBodyDef();
            switch (desc.Type)
            {
            case PhysicsBodyType3D::Static:
                bodyDef.type = b3_staticBody;
                break;
            case PhysicsBodyType3D::Dynamic:
                bodyDef.type = b3_dynamicBody;
                break;
            case PhysicsBodyType3D::Kinematic:
                bodyDef.type = b3_kinematicBody;
                break;
            }
            bodyDef.position = ToBoxPosition(desc.Position);
            bodyDef.rotation = ToBox(desc.Rotation);
            bodyDef.linearVelocity = ToBox(desc.LinearVelocity);
            bodyDef.angularVelocity = ToBox(desc.AngularVelocity);
            bodyDef.linearDamping = std::max(desc.LinearDamping, 0.0f);
            bodyDef.angularDamping = std::max(desc.AngularDamping, 0.0f);
            bodyDef.gravityScale = desc.GravityScale;
            bodyDef.enableSleep = desc.AllowSleep;
            bodyDef.isAwake = desc.StartAwake;
            bodyDef.isBullet = desc.Continuous;
            bodyDef.motionLocks.angularX = desc.LockRotationX;
            bodyDef.motionLocks.angularY = desc.LockRotationY;
            bodyDef.motionLocks.angularZ = desc.LockRotationZ;
            bodyDef.userData = reinterpret_cast<void*>(handle.Value);

            const b3BodyId nativeBody = b3CreateBody(m_World, &bodyDef);
            if (B3_IS_NULL(nativeBody))
                return {};
            m_Bodies.emplace(handle, BodyRecord{ nativeBody, desc });
            return handle;
        }

        void DestroyBody(PhysicsBody3DHandle body) override
        {
            auto bodyIt = m_Bodies.find(body);
            if (bodyIt == m_Bodies.end())
                return;

            Vector<PhysicsShape3DHandle> shapes;
            for (const auto& [shapeHandle, shape] : m_Shapes)
            {
                if (shape.Body == body)
                    shapes.push_back(shapeHandle);
            }
            for (PhysicsShape3DHandle shape : shapes)
                RemoveShape(body, shape);
            b3DestroyBody(bodyIt->second.Native);
            m_Bodies.erase(bodyIt);
        }

        PhysicsShape3DHandle AddShape(PhysicsBody3DHandle body, const PhysicsShape3DDesc& desc) override
        {
            if (m_Bodies.find(body) == m_Bodies.end())
                return {};
            PhysicsShape3DHandle handle{ NextHandle() };
            ShapeRecord record;
            record.Body = body;
            record.Desc = desc;
            if (!CreateNativeShape(handle, record))
                return {};
            m_NativeShapes[b3StoreShapeId(record.Native)] = handle;
            m_Shapes.emplace(handle, std::move(record));
            ApplyMass(body);
            return handle;
        }

        void RemoveShape(PhysicsBody3DHandle body, PhysicsShape3DHandle shape) override
        {
            auto shapeIt = m_Shapes.find(shape);
            if (shapeIt == m_Shapes.end() || shapeIt->second.Body != body)
                return;
            RemoveActiveContacts(shape);
            if (b3Shape_IsValid(shapeIt->second.Native))
                b3DestroyShape(shapeIt->second.Native, false);
            m_NativeShapes.erase(b3StoreShapeId(shapeIt->second.Native));
            DestroyShapeResource(shapeIt->second);
            m_Shapes.erase(shapeIt);
            ApplyMass(body);
        }

        void SetBodyTransform(PhysicsBody3DHandle body, const glm::vec3& position, const glm::quat& rotation, bool activate) override
        {
            if (BodyRecord* record = FindBody(body))
            {
                b3Body_SetTransform(record->Native, ToBoxPosition(position), ToBox(rotation));
                if (activate)
                    b3Body_SetAwake(record->Native, true);
            }
        }

        void GetBodyTransform(PhysicsBody3DHandle body, glm::vec3& position, glm::quat& rotation) const override
        {
            if (const BodyRecord* record = FindBody(body))
            {
                const b3WorldTransform transform = b3Body_GetTransform(record->Native);
                position = FromBoxPosition(transform.p);
                rotation = FromBox(transform.q);
            }
        }

        void MoveKinematic(PhysicsBody3DHandle body, const glm::vec3& position, const glm::quat& rotation, float timestep) override
        {
            if (BodyRecord* record = FindBody(body))
                b3Body_SetTargetTransform(record->Native, { ToBoxPosition(position), ToBox(rotation) }, timestep, true);
        }

        void SetLinearVelocity(PhysicsBody3DHandle body, const glm::vec3& velocity) override
        {
            if (BodyRecord* record = FindBody(body))
                b3Body_SetLinearVelocity(record->Native, ToBox(velocity));
        }

        glm::vec3 GetLinearVelocity(PhysicsBody3DHandle body) const override
        {
            const BodyRecord* record = FindBody(body);
            return record ? FromBox(b3Body_GetLinearVelocity(record->Native)) : glm::vec3(0.0f);
        }

        void SetAngularVelocity(PhysicsBody3DHandle body, const glm::vec3& velocity) override
        {
            if (BodyRecord* record = FindBody(body))
                b3Body_SetAngularVelocity(record->Native, ToBox(velocity));
        }

        glm::vec3 GetAngularVelocity(PhysicsBody3DHandle body) const override
        {
            const BodyRecord* record = FindBody(body);
            return record ? FromBox(b3Body_GetAngularVelocity(record->Native)) : glm::vec3(0.0f);
        }

        void AddForce(PhysicsBody3DHandle body, const glm::vec3& force, const glm::vec3* point, PhysicsForceMode3D mode) override
        {
            BodyRecord* record = FindBody(body);
            if (!record)
                return;
            const float mass = std::max(b3Body_GetMass(record->Native), 0.0f);
            glm::vec3 value = force;
            if (mode == PhysicsForceMode3D::Acceleration || mode == PhysicsForceMode3D::VelocityChange)
                value *= mass;
            if (mode == PhysicsForceMode3D::Impulse || mode == PhysicsForceMode3D::VelocityChange)
            {
                if (point)
                    b3Body_ApplyLinearImpulse(record->Native, ToBox(value), ToBoxPosition(*point), true);
                else
                    b3Body_ApplyLinearImpulseToCenter(record->Native, ToBox(value), true);
            }
            else if (point)
                b3Body_ApplyForce(record->Native, ToBox(value), ToBoxPosition(*point), true);
            else
                b3Body_ApplyForceToCenter(record->Native, ToBox(value), true);
        }

        void AddTorque(PhysicsBody3DHandle body, const glm::vec3& torque, PhysicsForceMode3D mode) override
        {
            BodyRecord* record = FindBody(body);
            if (!record)
                return;
            glm::vec3 value = torque;
            if (mode == PhysicsForceMode3D::Acceleration || mode == PhysicsForceMode3D::VelocityChange)
                value *= std::max(b3Body_GetMass(record->Native), 0.0f);
            if (mode == PhysicsForceMode3D::Impulse || mode == PhysicsForceMode3D::VelocityChange)
                b3Body_ApplyAngularImpulse(record->Native, ToBox(value), true);
            else
                b3Body_ApplyTorque(record->Native, ToBox(value), true);
        }

        void SetGravityScale(PhysicsBody3DHandle body, float scale) override
        {
            if (BodyRecord* record = FindBody(body))
                b3Body_SetGravityScale(record->Native, scale);
        }

        void SetDamping(PhysicsBody3DHandle body, float linear, float angular) override
        {
            if (BodyRecord* record = FindBody(body))
            {
                b3Body_SetLinearDamping(record->Native, std::max(linear, 0.0f));
                b3Body_SetAngularDamping(record->Native, std::max(angular, 0.0f));
            }
        }

        void SetAwake(PhysicsBody3DHandle body, bool awake) override
        {
            if (BodyRecord* record = FindBody(body))
                b3Body_SetAwake(record->Native, awake);
        }

        bool IsAwake(PhysicsBody3DHandle body) const override
        {
            const BodyRecord* record = FindBody(body);
            return record && b3Body_IsAwake(record->Native);
        }

        void SetFilter(PhysicsBody3DHandle body, const PhysicsFilter3D& filter) override
        {
            for (auto& [handle, shape] : m_Shapes)
            {
                if (shape.Body != body)
                    continue;
                shape.Desc.Filter = filter;
                b3Shape_SetFilter(shape.Native, ToBox(filter), true);
            }
        }

        void SetShapeMaterial(PhysicsShape3DHandle shape, const PhysicsMaterialData& material) override
        {
            auto it = m_Shapes.find(shape);
            if (it == m_Shapes.end())
                return;
            const PhysicsMaterialData normalized = NormalizePhysicsMaterialData(material);
            it->second.Desc.Material = normalized;
            b3Shape_SetDensity(it->second.Native, normalized.Density, true);
            b3SurfaceMaterial surface = b3Shape_GetSurfaceMaterial(it->second.Native);
            surface.friction = normalized.Friction;
            surface.restitution = normalized.Restitution;
            surface.userMaterialId = PackMaterialModes(normalized);
            b3Shape_SetSurfaceMaterial(it->second.Native, surface);
        }

        void SetShapeTrigger(PhysicsShape3DHandle shape, bool trigger) override
        {
            auto it = m_Shapes.find(shape);
            if (it == m_Shapes.end() || it->second.Desc.IsTrigger == trigger)
                return;
            RemoveActiveContacts(shape);
            m_NativeShapes.erase(b3StoreShapeId(it->second.Native));
            b3DestroyShape(it->second.Native, false);
            DestroyShapeResource(it->second);
            it->second.Desc.IsTrigger = trigger;
            if (CreateNativeShape(shape, it->second))
                m_NativeShapes[b3StoreShapeId(it->second.Native)] = shape;
            ApplyMass(it->second.Body);
        }

        PhysicsConstraint3DHandle CreateConstraint(const PhysicsConstraint3DDesc& desc) override
        {
            BodyRecord* bodyA = FindBody(desc.BodyA);
            BodyRecord* bodyB = FindBody(desc.BodyB);
            if (!bodyA || !bodyB)
                return {};

            const PhysicsConstraint3DHandle handle{ NextHandle() };
            b3JointId native = b3_nullJointId;
            switch (desc.Type)
            {
            case PhysicsConstraintType3D::Fixed: {
                b3WeldJointDef def = b3DefaultWeldJointDef();
                SetJointBase(def.base, handle, desc, bodyA->Native, bodyB->Native, glm::vec3(1, 0, 0));
                def.linearHertz = desc.EnableSpring ? desc.Frequency : 0.0f;
                def.angularHertz = def.linearHertz;
                def.linearDampingRatio = desc.Damping;
                def.angularDampingRatio = desc.Damping;
                native = b3CreateWeldJoint(m_World, &def);
                break;
            }
            case PhysicsConstraintType3D::Point: {
                b3SphericalJointDef def = b3DefaultSphericalJointDef();
                SetJointBase(def.base, handle, desc, bodyA->Native, bodyB->Native, glm::vec3(0, 0, 1));
                native = b3CreateSphericalJoint(m_World, &def);
                break;
            }
            case PhysicsConstraintType3D::Distance:
            case PhysicsConstraintType3D::Spring: {
                b3DistanceJointDef def = b3DefaultDistanceJointDef();
                SetJointBase(def.base, handle, desc, bodyA->Native, bodyB->Native, glm::vec3(1, 0, 0));
                def.length = std::max(desc.MaximumLimit, 0.001f);
                def.enableLimit = desc.EnableLimits;
                def.minLength = std::max(desc.MinimumLimit, 0.001f);
                def.maxLength = std::max(desc.MaximumLimit, def.minLength);
                def.enableSpring = desc.Type == PhysicsConstraintType3D::Spring || desc.EnableSpring;
                def.hertz = desc.Frequency;
                def.dampingRatio = desc.Damping;
                def.enableMotor = desc.EnableMotor;
                def.motorSpeed = desc.MotorTargetVelocity;
                def.maxMotorForce = desc.MaximumMotorForce;
                native = b3CreateDistanceJoint(m_World, &def);
                break;
            }
            case PhysicsConstraintType3D::Hinge: {
                b3RevoluteJointDef def = b3DefaultRevoluteJointDef();
                SetJointBase(def.base, handle, desc, bodyA->Native, bodyB->Native, glm::vec3(0, 0, 1));
                def.enableLimit = desc.EnableLimits;
                def.lowerAngle = desc.MinimumLimit;
                def.upperAngle = desc.MaximumLimit;
                def.enableSpring = desc.EnableSpring;
                def.hertz = desc.Frequency;
                def.dampingRatio = desc.Damping;
                def.enableMotor = desc.EnableMotor;
                def.motorSpeed = desc.MotorTargetVelocity;
                def.maxMotorTorque = desc.MaximumMotorForce;
                native = b3CreateRevoluteJoint(m_World, &def);
                break;
            }
            case PhysicsConstraintType3D::Slider: {
                b3PrismaticJointDef def = b3DefaultPrismaticJointDef();
                SetJointBase(def.base, handle, desc, bodyA->Native, bodyB->Native, glm::vec3(1, 0, 0));
                def.enableLimit = desc.EnableLimits;
                def.lowerTranslation = desc.MinimumLimit;
                def.upperTranslation = desc.MaximumLimit;
                def.enableSpring = desc.EnableSpring;
                def.hertz = desc.Frequency;
                def.dampingRatio = desc.Damping;
                def.enableMotor = desc.EnableMotor;
                def.motorSpeed = desc.MotorTargetVelocity;
                def.maxMotorForce = desc.MaximumMotorForce;
                native = b3CreatePrismaticJoint(m_World, &def);
                break;
            }
            case PhysicsConstraintType3D::ConeTwist: {
                b3SphericalJointDef def = b3DefaultSphericalJointDef();
                SetJointBase(def.base, handle, desc, bodyA->Native, bodyB->Native, glm::vec3(0, 0, 1));
                def.enableConeLimit = desc.EnableLimits;
                def.coneAngle = std::max(std::abs(desc.MinimumLimit), std::abs(desc.MaximumLimit));
                def.enableTwistLimit = desc.EnableLimits;
                def.lowerTwistAngle = desc.MinimumLimit;
                def.upperTwistAngle = desc.MaximumLimit;
                def.enableSpring = desc.EnableSpring;
                def.hertz = desc.Frequency;
                def.dampingRatio = desc.Damping;
                def.enableMotor = desc.EnableMotor;
                def.motorVelocity = ToBox(desc.AxisA * desc.MotorTargetVelocity);
                def.maxMotorTorque = desc.MaximumMotorForce;
                native = b3CreateSphericalJoint(m_World, &def);
                break;
            }
            }
            if (B3_IS_NULL(native))
                return {};
            m_Constraints.emplace(handle.Value, native);
            return handle;
        }

        void DestroyConstraint(PhysicsConstraint3DHandle constraint) override
        {
            auto it = m_Constraints.find(constraint.Value);
            if (it == m_Constraints.end())
                return;
            if (b3Joint_IsValid(it->second))
                b3DestroyJoint(it->second, true);
            m_Constraints.erase(it);
        }

        Vector<PhysicsQueryHit3D> Raycast(const glm::vec3& origin, const glm::vec3& direction, float distance,
                                          const PhysicsQueryFilter3D& filter) const override
        {
            QueryContext context{ this, filter, distance };
            const glm::vec3 translation = glm::normalize(direction) * distance;
            b3World_CastRay(m_World, ToBoxPosition(origin), ToBox(translation), ToBox(filter), QueryCallback, &context);
            SortHits(context.Hits);
            return context.Hits;
        }

        Vector<PhysicsQueryHit3D> Sweep(const PhysicsShape3DDesc& shape, const glm::vec3& position, const glm::quat& rotation,
                                        const glm::vec3& direction, float distance, const PhysicsQueryFilter3D& filter) const override
        {
            ProxyStorage storage;
            if (!BuildProxy(shape, rotation, storage))
                return {};
            QueryContext context{ this, filter, distance };
            const glm::vec3 translation = glm::normalize(direction) * distance;
            b3World_CastShape(m_World, ToBoxPosition(position + rotation * shape.LocalPosition), &storage.Proxy, ToBox(translation), ToBox(filter),
                              QueryCallback, &context);
            SortHits(context.Hits);
            return context.Hits;
        }

        Vector<PhysicsQueryHit3D> Overlap(const PhysicsShape3DDesc& shape, const glm::vec3& position, const glm::quat& rotation,
                                          const PhysicsQueryFilter3D& filter) const override
        {
            ProxyStorage storage;
            if (!BuildProxy(shape, rotation, storage))
                return {};
            QueryContext context{ this, filter, 0.0f };
            b3World_OverlapShape(m_World, ToBoxPosition(position + rotation * shape.LocalPosition), &storage.Proxy, ToBox(filter), OverlapCallback,
                                 &context);
            return context.Hits;
        }

    private:
        struct BodyRecord
        {
            b3BodyId Native = b3_nullBodyId;
            PhysicsBody3DDesc Desc;
        };

        struct ShapeRecord
        {
            b3ShapeId Native = b3_nullShapeId;
            PhysicsBody3DHandle Body;
            PhysicsShape3DDesc Desc;
            b3MeshData* Mesh = nullptr;
            b3HeightFieldData* HeightField = nullptr;
        };

        struct ActiveContact
        {
            PhysicsShape3DHandle ShapeA;
            PhysicsShape3DHandle ShapeB;
            bool Trigger = false;
        };

        struct ProxyStorage
        {
            Vector<b3Vec3> Points;
            b3ShapeProxy Proxy{};
        };

        struct QueryContext
        {
            const Box3DPhysicsBackend* Backend;
            PhysicsQueryFilter3D Filter;
            float Distance;
            Vector<PhysicsQueryHit3D> Hits;
        };

        uint64_t NextHandle() { return m_NextHandle++; }

        BodyRecord* FindBody(PhysicsBody3DHandle handle)
        {
            auto it = m_Bodies.find(handle);
            return it == m_Bodies.end() ? nullptr : &it->second;
        }

        const BodyRecord* FindBody(PhysicsBody3DHandle handle) const
        {
            auto it = m_Bodies.find(handle);
            return it == m_Bodies.end() ? nullptr : &it->second;
        }

        PhysicsShape3DHandle FindShape(b3ShapeId native) const
        {
            auto it = m_NativeShapes.find(b3StoreShapeId(native));
            return it == m_NativeShapes.end() ? PhysicsShape3DHandle{} : it->second;
        }

        PhysicsBody3DHandle FindShapeBody(PhysicsShape3DHandle shape) const
        {
            auto it = m_Shapes.find(shape);
            return it == m_Shapes.end() ? PhysicsBody3DHandle{} : it->second.Body;
        }

        bool AcceptShape(PhysicsShape3DHandle shape, const PhysicsQueryFilter3D& filter) const
        {
            auto it = m_Shapes.find(shape);
            return it != m_Shapes.end() && (!filter.IgnoreBody || it->second.Body != filter.IgnoreBody) &&
                   (filter.IncludeTriggers || !it->second.Desc.IsTrigger);
        }

        bool CreateNativeShape(PhysicsShape3DHandle handle, ShapeRecord& record)
        {
            BodyRecord* body = FindBody(record.Body);
            if (!body)
                return false;
            const PhysicsShape3DDesc& desc = record.Desc;
            b3ShapeDef shapeDef = b3DefaultShapeDef();
            shapeDef.userData = reinterpret_cast<void*>(handle.Value);
            shapeDef.density = std::max(desc.Material.Density, 0.0f);
            shapeDef.baseMaterial.friction = std::max(desc.Material.Friction, 0.0f);
            shapeDef.baseMaterial.restitution = std::clamp(desc.Material.Restitution, 0.0f, 1.0f);
            shapeDef.baseMaterial.userMaterialId = PackMaterialModes(desc.Material);
            shapeDef.filter = ToBox(desc.Filter);
            shapeDef.isSensor = desc.IsTrigger;
            shapeDef.enableSensorEvents = true;
            shapeDef.enableContactEvents = true;
            shapeDef.enableHitEvents = true;

            const b3Transform localTransform{ ToBox(desc.LocalPosition), ToBox(desc.LocalRotation) };
            switch (desc.Type)
            {
            case PhysicsShapeType3D::Box: {
                const glm::vec3 extents = glm::max(glm::abs(desc.HalfExtents), glm::vec3(0.001f));
                b3BoxHull hull = b3MakeBoxHull(extents.x, extents.y, extents.z);
                record.Native = b3CreateTransformedHullShape(body->Native, &shapeDef, &hull.base, localTransform, { 1.0f, 1.0f, 1.0f });
                break;
            }
            case PhysicsShapeType3D::Sphere: {
                const b3Sphere sphere{ ToBox(desc.LocalPosition), std::max(desc.Radius, 0.001f) };
                record.Native = b3CreateSphereShape(body->Native, &shapeDef, &sphere);
                break;
            }
            case PhysicsShapeType3D::Capsule: {
                const float halfSegment = std::max(desc.Height * 0.5f - desc.Radius, 0.0f);
                const glm::vec3 axis = desc.LocalRotation * glm::vec3(0.0f, halfSegment, 0.0f);
                const b3Capsule capsule{ ToBox(desc.LocalPosition - axis), ToBox(desc.LocalPosition + axis), std::max(desc.Radius, 0.001f) };
                record.Native = b3CreateCapsuleShape(body->Native, &shapeDef, &capsule);
                break;
            }
            case PhysicsShapeType3D::ConvexHull: {
                if (desc.Vertices.size() < 4)
                    return false;
                Vector<b3Vec3> points;
                points.reserve(desc.Vertices.size());
                for (const glm::vec3& point : desc.Vertices)
                    points.push_back(ToBox(point));
                b3HullData* hull =
                  b3CreateHull(points.data(), static_cast<int>(points.size()), static_cast<int>(std::min<size_t>(points.size(), 255)));
                if (!hull)
                    return false;
                record.Native = b3CreateTransformedHullShape(body->Native, &shapeDef, hull, localTransform, { 1.0f, 1.0f, 1.0f });
                b3DestroyHull(hull);
                break;
            }
            case PhysicsShapeType3D::TriangleMesh: {
                if (body->Desc.Type != PhysicsBodyType3D::Static || desc.Vertices.size() < 3 || desc.Indices.size() < 3 ||
                    desc.Indices.size() % 3 != 0)
                    return false;
                Vector<b3Vec3> vertices;
                vertices.reserve(desc.Vertices.size());
                for (const glm::vec3& point : desc.Vertices)
                    vertices.push_back(ToBox(desc.LocalPosition + desc.LocalRotation * point));
                Vector<int32_t> indices(desc.Indices.begin(), desc.Indices.end());
                b3MeshDef meshDef{};
                meshDef.vertices = vertices.data();
                meshDef.indices = indices.data();
                meshDef.vertexCount = static_cast<int>(vertices.size());
                meshDef.triangleCount = static_cast<int>(indices.size() / 3);
                meshDef.identifyEdges = true;
                record.Mesh = b3CreateMesh(&meshDef, nullptr, 0);
                if (!record.Mesh)
                    return false;
                record.Native = b3CreateMeshShape(body->Native, &shapeDef, record.Mesh, { 1.0f, 1.0f, 1.0f });
                break;
            }
            case PhysicsShapeType3D::HeightField: {
                if (body->Desc.Type != PhysicsBodyType3D::Static || desc.HeightFieldRows < 2 || desc.HeightFieldColumns < 2 ||
                    desc.Heights.size() != size_t(desc.HeightFieldRows) * desc.HeightFieldColumns)
                    return false;
                b3HeightFieldDef fieldDef{};
                fieldDef.heights = const_cast<float*>(desc.Heights.data());
                fieldDef.countX = static_cast<int>(desc.HeightFieldColumns);
                fieldDef.countZ = static_cast<int>(desc.HeightFieldRows);
                fieldDef.scale = ToBox(glm::max(desc.HeightFieldScale, glm::vec3(0.001f)));
                const auto range = std::minmax_element(desc.Heights.begin(), desc.Heights.end());
                fieldDef.globalMinimumHeight = *range.first;
                fieldDef.globalMaximumHeight = *range.second;
                record.HeightField = b3CreateHeightField(&fieldDef);
                if (!record.HeightField)
                    return false;
                record.Native = b3CreateHeightFieldShape(body->Native, &shapeDef, record.HeightField);
                break;
            }
            }
            return B3_IS_NON_NULL(record.Native);
        }

        static void DestroyShapeResource(ShapeRecord& record)
        {
            if (record.Mesh)
                b3DestroyMesh(record.Mesh);
            if (record.HeightField)
                b3DestroyHeightField(record.HeightField);
            record.Mesh = nullptr;
            record.HeightField = nullptr;
        }

        void ApplyMass(PhysicsBody3DHandle bodyHandle)
        {
            BodyRecord* body = FindBody(bodyHandle);
            if (!body || body->Desc.Type != PhysicsBodyType3D::Dynamic)
                return;
            b3Body_ApplyMassFromShapes(body->Native);
            if (body->Desc.AutoMass)
                return;
            b3MassData mass = b3Body_GetMassData(body->Native);
            const float oldMass = std::max(mass.mass, 0.0001f);
            const float newMass = std::max(body->Desc.Mass, 0.0001f);
            const float scale = newMass / oldMass;
            mass.mass = newMass;
            mass.center = ToBox(body->Desc.CenterOfMass);
            mass.inertia.cx = { mass.inertia.cx.x * scale, mass.inertia.cx.y * scale, mass.inertia.cx.z * scale };
            mass.inertia.cy = { mass.inertia.cy.x * scale, mass.inertia.cy.y * scale, mass.inertia.cy.z * scale };
            mass.inertia.cz = { mass.inertia.cz.x * scale, mass.inertia.cz.y * scale, mass.inertia.cz.z * scale };
            b3Body_SetMassData(body->Native, mass);
        }

        void SetJointBase(b3JointDef& base, PhysicsConstraint3DHandle handle, const PhysicsConstraint3DDesc& desc, b3BodyId bodyA, b3BodyId bodyB,
                          const glm::vec3& referenceAxis)
        {
            base.bodyIdA = bodyA;
            base.bodyIdB = bodyB;
            base.localFrameA = MakeFrame(desc.AnchorA, desc.AxisA, referenceAxis);
            base.localFrameB = MakeFrame(desc.AnchorB, desc.AxisB, referenceAxis);
            base.forceThreshold = desc.BreakForce;
            base.torqueThreshold = desc.BreakTorque;
            base.collideConnected = desc.EnableCollision;
            base.userData = reinterpret_cast<void*>(handle.Value);
        }

        static bool BuildProxy(const PhysicsShape3DDesc& shape, const glm::quat& worldRotation, ProxyStorage& storage)
        {
            const glm::quat rotation = worldRotation * shape.LocalRotation;
            switch (shape.Type)
            {
            case PhysicsShapeType3D::Sphere:
                storage.Points.push_back({ 0.0f, 0.0f, 0.0f });
                storage.Proxy.radius = std::max(shape.Radius, 0.001f);
                break;
            case PhysicsShapeType3D::Capsule: {
                const float halfSegment = std::max(shape.Height * 0.5f - shape.Radius, 0.0f);
                storage.Points.push_back(ToBox(rotation * glm::vec3(0.0f, -halfSegment, 0.0f)));
                storage.Points.push_back(ToBox(rotation * glm::vec3(0.0f, halfSegment, 0.0f)));
                storage.Proxy.radius = std::max(shape.Radius, 0.001f);
                break;
            }
            case PhysicsShapeType3D::Box:
                for (int x : { -1, 1 })
                    for (int y : { -1, 1 })
                        for (int z : { -1, 1 })
                            storage.Points.push_back(ToBox(rotation * (shape.HalfExtents * glm::vec3(x, y, z))));
                break;
            case PhysicsShapeType3D::ConvexHull:
                for (const glm::vec3& point : shape.Vertices)
                    storage.Points.push_back(ToBox(rotation * point));
                break;
            default:
                return false;
            }
            if (storage.Points.empty() || storage.Points.size() > B3_MAX_SHAPE_CAST_POINTS)
                return false;
            storage.Proxy.points = storage.Points.data();
            storage.Proxy.count = static_cast<int>(storage.Points.size());
            return true;
        }

        static float QueryCallback(b3ShapeId nativeShape, b3Pos point, b3Vec3 normal, float fraction, uint64_t userMaterialId,
                                   CW_MAYBE_UNUSED int triangleIndex, CW_MAYBE_UNUSED int childIndex, void* userContext)
        {
            auto& context = *static_cast<QueryContext*>(userContext);
            const PhysicsShape3DHandle shape = context.Backend->FindShape(nativeShape);
            if (!shape || !context.Backend->AcceptShape(shape, context.Filter))
                return -1.0f;
            const auto shapeIt = context.Backend->m_Shapes.find(shape);
            PhysicsQueryHit3D hit;
            hit.Shape = shape;
            hit.Body = shapeIt->second.Body;
            hit.Point = FromBoxPosition(point);
            hit.Normal = FromBox(normal);
            hit.Fraction = fraction;
            hit.Distance = fraction * context.Distance;
            hit.UserData = shapeIt->second.Desc.UserData ? shapeIt->second.Desc.UserData : userMaterialId;
            context.Hits.push_back(hit);
            return 1.0f;
        }

        static bool OverlapCallback(b3ShapeId nativeShape, void* userContext)
        {
            auto& context = *static_cast<QueryContext*>(userContext);
            const PhysicsShape3DHandle shape = context.Backend->FindShape(nativeShape);
            if (!shape || !context.Backend->AcceptShape(shape, context.Filter))
                return true;
            const auto shapeIt = context.Backend->m_Shapes.find(shape);
            PhysicsQueryHit3D hit;
            hit.Shape = shape;
            hit.Body = shapeIt->second.Body;
            hit.UserData = shapeIt->second.Desc.UserData;
            context.Hits.push_back(hit);
            return true;
        }

        static void SortHits(Vector<PhysicsQueryHit3D>& hits)
        {
            std::sort(hits.begin(), hits.end(),
                      [](const PhysicsQueryHit3D& lhs, const PhysicsQueryHit3D& rhs) { return lhs.Fraction < rhs.Fraction; });
        }

        PhysicsContactEvent3D MakeEvent(PhysicsContactEventType3D type, PhysicsShape3DHandle shapeA, PhysicsShape3DHandle shapeB, bool trigger) const
        {
            PhysicsContactEvent3D event;
            event.Type = type;
            event.ShapeA = shapeA;
            event.ShapeB = shapeB;
            event.BodyA = FindShapeBody(shapeA);
            event.BodyB = FindShapeBody(shapeB);
            event.IsTrigger = trigger;
            const auto shapeAIt = m_Shapes.find(shapeA);
            const auto shapeBIt = m_Shapes.find(shapeB);
            if (shapeAIt != m_Shapes.end())
            {
                event.ShapeUserDataA = shapeAIt->second.Desc.UserData;
                event.MaterialA = shapeAIt->second.Desc.Material;
            }
            if (shapeBIt != m_Shapes.end())
            {
                event.ShapeUserDataB = shapeBIt->second.Desc.UserData;
                event.MaterialB = shapeBIt->second.Desc.Material;
            }
            if (trigger || !shapeA || !shapeB)
                return event;

            if (shapeAIt == m_Shapes.end() || !b3Shape_IsValid(shapeAIt->second.Native))
                return event;
            const int count = b3Shape_GetContactCapacity(shapeAIt->second.Native);
            if (count <= 0)
                return event;
            Vector<b3ContactData> contacts(count);
            const int written = b3Shape_GetContactData(shapeAIt->second.Native, contacts.data(), count);
            for (int i = 0; i < written; ++i)
            {
                const b3ContactData& contact = contacts[i];
                if (FindShape(contact.shapeIdA) != shapeB && FindShape(contact.shapeIdB) != shapeB)
                    continue;
                const b3Pos center = b3Body_GetWorldCenterOfMass(b3Shape_GetBody(contact.shapeIdA));
                for (int manifoldIndex = 0; manifoldIndex < contact.manifoldCount; ++manifoldIndex)
                {
                    const b3Manifold& manifold = contact.manifolds[manifoldIndex];
                    for (int pointIndex = 0; pointIndex < manifold.pointCount; ++pointIndex)
                    {
                        const b3ManifoldPoint& point = manifold.points[pointIndex];
                        PhysicsContactPoint3D output;
                        output.Point = FromBoxPosition(center) + FromBox(point.anchorA);
                        output.Normal = FromBox(manifold.normal);
                        output.Separation = point.separation;
                        output.NormalImpulse = point.totalNormalImpulse;
                        event.Points.push_back(output);
                    }
                }
            }
            return event;
        }

        void SnapshotEvents()
        {
            m_ContactEvents.clear();
            const b3ContactEvents contacts = b3World_GetContactEvents(m_World);
            for (int i = 0; i < contacts.beginCount; ++i)
            {
                const PhysicsShape3DHandle shapeA = FindShape(contacts.beginEvents[i].shapeIdA);
                const PhysicsShape3DHandle shapeB = FindShape(contacts.beginEvents[i].shapeIdB);
                if (!shapeA || !shapeB)
                    continue;
                m_ActiveContacts[MakePair(shapeA.Value, shapeB.Value)] = { shapeA, shapeB, false };
                m_ContactEvents.push_back(MakeEvent(PhysicsContactEventType3D::Enter, shapeA, shapeB, false));
            }
            for (int i = 0; i < contacts.endCount; ++i)
            {
                const PhysicsShape3DHandle shapeA = FindShape(contacts.endEvents[i].shapeIdA);
                const PhysicsShape3DHandle shapeB = FindShape(contacts.endEvents[i].shapeIdB);
                const ContactPair pair = MakePair(shapeA.Value, shapeB.Value);
                auto active = m_ActiveContacts.find(pair);
                if (active == m_ActiveContacts.end())
                    continue;
                m_ContactEvents.push_back(MakeEvent(PhysicsContactEventType3D::Exit, active->second.ShapeA, active->second.ShapeB, false));
                m_ActiveContacts.erase(active);
            }

            const b3SensorEvents sensors = b3World_GetSensorEvents(m_World);
            for (int i = 0; i < sensors.beginCount; ++i)
            {
                const PhysicsShape3DHandle shapeA = FindShape(sensors.beginEvents[i].sensorShapeId);
                const PhysicsShape3DHandle shapeB = FindShape(sensors.beginEvents[i].visitorShapeId);
                if (!shapeA || !shapeB)
                    continue;
                m_ActiveContacts[MakePair(shapeA.Value, shapeB.Value)] = { shapeA, shapeB, true };
                m_ContactEvents.push_back(MakeEvent(PhysicsContactEventType3D::Enter, shapeA, shapeB, true));
            }
            for (int i = 0; i < sensors.endCount; ++i)
            {
                const PhysicsShape3DHandle shapeA = FindShape(sensors.endEvents[i].sensorShapeId);
                const PhysicsShape3DHandle shapeB = FindShape(sensors.endEvents[i].visitorShapeId);
                const ContactPair pair = MakePair(shapeA.Value, shapeB.Value);
                auto active = m_ActiveContacts.find(pair);
                if (active == m_ActiveContacts.end())
                    continue;
                m_ContactEvents.push_back(MakeEvent(PhysicsContactEventType3D::Exit, active->second.ShapeA, active->second.ShapeB, true));
                m_ActiveContacts.erase(active);
            }

            for (const auto& [pair, active] : m_ActiveContacts)
                m_ContactEvents.push_back(MakeEvent(PhysicsContactEventType3D::Stay, active.ShapeA, active.ShapeB, active.Trigger));
            NormalizePhysicsContactEvents3D(m_ContactEvents);
            if (m_Callback)
            {
                for (const PhysicsContactEvent3D& event : m_ContactEvents)
                    m_Callback(event);
            }
            m_ContactEvents.clear();
        }

        void RemoveActiveContacts(PhysicsShape3DHandle shape)
        {
            for (auto it = m_ActiveContacts.begin(); it != m_ActiveContacts.end();)
            {
                if (it->second.ShapeA == shape || it->second.ShapeB == shape)
                    it = m_ActiveContacts.erase(it);
                else
                    ++it;
            }
        }

        b3WorldId m_World = b3_nullWorldId;
        PhysicsContactCallback3D m_Callback;
        uint64_t m_NextHandle = 1;
        UnorderedMap<PhysicsBody3DHandle, BodyRecord> m_Bodies;
        UnorderedMap<PhysicsShape3DHandle, ShapeRecord> m_Shapes;
        UnorderedMap<uint64_t, PhysicsShape3DHandle> m_NativeShapes;
        UnorderedMap<uint64_t, b3JointId> m_Constraints;
        std::unordered_map<ContactPair, ActiveContact, ContactPairHash> m_ActiveContacts;
        Vector<PhysicsContactEvent3D> m_ContactEvents;
    };

    Scope<Physics3DBackend> CreateBox3DBackend() { return CreateScope<Box3DPhysicsBackend>(); }
} // namespace Crowny

#else

namespace Crowny
{
    Scope<Physics3DBackend> CreateBox3DBackend() { return nullptr; }
} // namespace Crowny

#endif

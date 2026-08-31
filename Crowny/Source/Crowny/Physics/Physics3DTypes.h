#pragma once

#include "Crowny/Common/StdHeaders.h"
#include "Crowny/Physics/PhysicsMaterial.h"
#include "Crowny/Utils/SmallVector.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <functional>
#include <limits>

namespace Crowny
{
    enum class Physics3DBackendType : uint8_t
    {
        Box3D = 0,
        Jolt = 1,
        Bullet = 2
    };

    enum class Physics3DCapability : uint64_t
    {
        None = 0,
        RigidBodies = 1ull << 0,
        PrimitiveShapes = 1ull << 1,
        ConvexShapes = 1ull << 2,
        TriangleMeshes = 1ull << 3,
        HeightFields = 1ull << 4,
        CompoundShapes = 1ull << 5,
        Sensors = 1ull << 6,
        ContinuousCollision = 1ull << 7,
        RayCasts = 1ull << 8,
        ShapeCasts = 1ull << 9,
        Overlaps = 1ull << 10,
        Constraints = 1ull << 11,
        Motors = 1ull << 12,
        Springs = 1ull << 13,
        CharacterController = 1ull << 14,
        SoftBodies = 1ull << 15,
        Vehicles = 1ull << 16,
        DeterministicSimulation = 1ull << 17
    };

    inline Physics3DCapability operator|(Physics3DCapability lhs, Physics3DCapability rhs)
    {
        return static_cast<Physics3DCapability>(static_cast<uint64_t>(lhs) | static_cast<uint64_t>(rhs));
    }

    inline Physics3DCapability& operator|=(Physics3DCapability& lhs, Physics3DCapability rhs)
    {
        lhs = lhs | rhs;
        return lhs;
    }

    inline bool HasCapability(Physics3DCapability capabilities, Physics3DCapability capability)
    {
        return (static_cast<uint64_t>(capabilities) & static_cast<uint64_t>(capability)) == static_cast<uint64_t>(capability);
    }

    struct PhysicsBody3DHandle
    {
        uint64_t Value = 0;
        explicit operator bool() const { return Value != 0; }
        bool operator==(const PhysicsBody3DHandle& other) const { return Value == other.Value; }
        bool operator!=(const PhysicsBody3DHandle& other) const { return Value != other.Value; }
    };

    struct PhysicsShape3DHandle
    {
        uint64_t Value = 0;
        explicit operator bool() const { return Value != 0; }
        bool operator==(const PhysicsShape3DHandle& other) const { return Value == other.Value; }
        bool operator!=(const PhysicsShape3DHandle& other) const { return Value != other.Value; }
    };

    struct PhysicsConstraint3DHandle
    {
        uint64_t Value = 0;
        explicit operator bool() const { return Value != 0; }
    };

    enum class PhysicsBodyType3D : uint8_t
    {
        Static,
        Dynamic,
        Kinematic
    };

    enum class PhysicsShapeType3D : uint8_t
    {
        Box,
        Sphere,
        Capsule,
        ConvexHull,
        TriangleMesh,
        HeightField
    };

    enum class PhysicsConstraintType3D : uint8_t
    {
        Fixed,
        Point,
        Distance,
        Hinge,
        Slider,
        ConeTwist,
        Spring
    };

    enum class PhysicsForceMode3D : uint8_t
    {
        Force,
        Impulse,
        VelocityChange,
        Acceleration
    };

    struct PhysicsFilter3D
    {
        uint32_t Layer = 0;
        uint32_t Mask = 0xFFFFFFFF;
        int32_t Group = 0;
    };

    struct PhysicsBody3DDesc
    {
        PhysicsBodyType3D Type = PhysicsBodyType3D::Static;
        glm::vec3 Position{ 0.0f };
        glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 LinearVelocity{ 0.0f };
        glm::vec3 AngularVelocity{ 0.0f };
        glm::vec3 CenterOfMass{ 0.0f };
        float Mass = 1.0f;
        float LinearDamping = 0.0f;
        float AngularDamping = 0.05f;
        float GravityScale = 1.0f;
        bool AutoMass = true;
        bool AllowSleep = true;
        bool StartAwake = true;
        bool Continuous = false;
        bool LockRotationX = false;
        bool LockRotationY = false;
        bool LockRotationZ = false;
        PhysicsFilter3D Filter;
        uint64_t UserData = 0;
    };

    struct PhysicsShape3DDesc
    {
        PhysicsShapeType3D Type = PhysicsShapeType3D::Box;
        glm::vec3 LocalPosition{ 0.0f };
        glm::quat LocalRotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 HalfExtents{ 0.5f };
        float Radius = 0.5f;
        float Height = 1.0f;
        bool IsTrigger = false;
        PhysicsMaterialData Material;
        PhysicsFilter3D Filter;
        Vector<glm::vec3> Vertices;
        Vector<uint32_t> Indices;
        Vector<float> Heights;
        uint32_t HeightFieldRows = 0;
        uint32_t HeightFieldColumns = 0;
        glm::vec3 HeightFieldScale{ 1.0f };
        uint64_t UserData = 0;
    };

    struct PhysicsConstraint3DDesc
    {
        PhysicsConstraintType3D Type = PhysicsConstraintType3D::Fixed;
        PhysicsBody3DHandle BodyA;
        PhysicsBody3DHandle BodyB;
        glm::vec3 AnchorA{ 0.0f };
        glm::vec3 AnchorB{ 0.0f };
        glm::vec3 AxisA{ 1.0f, 0.0f, 0.0f };
        glm::vec3 AxisB{ 1.0f, 0.0f, 0.0f };
        float MinimumLimit = 0.0f;
        float MaximumLimit = 0.0f;
        float Frequency = 0.0f;
        float Damping = 0.0f;
        float MotorTargetVelocity = 0.0f;
        float MaximumMotorForce = 0.0f;
        float BreakForce = std::numeric_limits<float>::max();
        float BreakTorque = std::numeric_limits<float>::max();
        bool EnableLimits = false;
        bool EnableSpring = false;
        bool EnableMotor = false;
        bool EnableCollision = false;
    };

    struct PhysicsQueryFilter3D
    {
        uint32_t LayerMask = 0xFFFFFFFF;
        bool IncludeTriggers = true;
        PhysicsBody3DHandle IgnoreBody;
    };

    struct PhysicsQueryHit3D
    {
        PhysicsBody3DHandle Body;
        PhysicsShape3DHandle Shape;
        glm::vec3 Point{ 0.0f };
        glm::vec3 Normal{ 0.0f };
        float Distance = 0.0f;
        float Fraction = 0.0f;
        uint64_t UserData = 0;
    };

    enum class PhysicsContactEventType3D : uint8_t
    {
        Enter,
        Stay,
        Exit
    };

    struct PhysicsContactPoint3D
    {
        glm::vec3 Point{ 0.0f };
        glm::vec3 Normal{ 0.0f };
        float Separation = 0.0f;
        float NormalImpulse = 0.0f;
    };

    struct PhysicsContactEvent3D
    {
        PhysicsContactEventType3D Type = PhysicsContactEventType3D::Enter;
        PhysicsBody3DHandle BodyA;
        PhysicsBody3DHandle BodyB;
        PhysicsShape3DHandle ShapeA;
        PhysicsShape3DHandle ShapeB;
        uint64_t ShapeUserDataA = 0;
        uint64_t ShapeUserDataB = 0;
        PhysicsMaterialData MaterialA;
        PhysicsMaterialData MaterialB;
        bool IsTrigger = false;
        SmallVector<PhysicsContactPoint3D, 4> Points;
    };

    using PhysicsContactCallback3D = std::function<void(const PhysicsContactEvent3D&)>;

    void NormalizePhysicsContactEvents3D(Vector<PhysicsContactEvent3D>& events);
} // namespace Crowny

namespace std
{
    template <> struct hash<Crowny::PhysicsBody3DHandle>
    {
        size_t operator()(const Crowny::PhysicsBody3DHandle& value) const { return hash<uint64_t>()(value.Value); }
    };

    template <> struct hash<Crowny::PhysicsShape3DHandle>
    {
        size_t operator()(const Crowny::PhysicsShape3DHandle& value) const { return hash<uint64_t>()(value.Value); }
    };
} // namespace std

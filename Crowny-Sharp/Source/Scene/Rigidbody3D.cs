using System;

namespace Crowny
{
    /// <summary>Controls how a force changes a 3D body.</summary>
    public enum ForceMode3D
    {
        Force = 0,
        Impulse = 1,
        VelocityChange = 2,
        Acceleration = 3
    }

    /// <summary>Locks selected degrees of freedom on a 3D body.</summary>
    [Flags]
    public enum Rigidbody3DConstraints : uint
    {
        None = 0,
        FreezeRotationX = 1,
        FreezeRotationY = 2,
        FreezeRotationZ = 4,
        FreezeRotation = FreezeRotationX | FreezeRotationY | FreezeRotationZ
    }

    /// <summary>A 3D rigid body controlled by the selected physics backend.</summary>
    public sealed class Rigidbody3D : Component
    {
        public BodyType BodyType { get { return (BodyType)ManagedRuntimeContext.Rigidbody3DGetBodyType(EntityId); } set { ManagedRuntimeContext.Rigidbody3DSetBodyType(EntityId, (int)value); } }
        public float Mass { get { return ManagedRuntimeContext.Rigidbody3DGetMass(EntityId); } set { ManagedRuntimeContext.Rigidbody3DSetMass(EntityId, value); } }
        public bool AutoMass { get { return ManagedRuntimeContext.Rigidbody3DGetAutoMass(EntityId); } set { ManagedRuntimeContext.Rigidbody3DSetAutoMass(EntityId, value); } }
        public float GravityScale { get { return ManagedRuntimeContext.Rigidbody3DGetGravityScale(EntityId); } set { ManagedRuntimeContext.Rigidbody3DSetGravityScale(EntityId, value); } }
        public float LinearDamping { get { return ManagedRuntimeContext.Rigidbody3DGetLinearDamping(EntityId); } set { ManagedRuntimeContext.Rigidbody3DSetLinearDamping(EntityId, value); } }
        public float AngularDamping { get { return ManagedRuntimeContext.Rigidbody3DGetAngularDamping(EntityId); } set { ManagedRuntimeContext.Rigidbody3DSetAngularDamping(EntityId, value); } }
        public Vector3 CenterOfMass { get { return ManagedRuntimeContext.Rigidbody3DGetCenterOfMass(EntityId); } set { ManagedRuntimeContext.Rigidbody3DSetCenterOfMass(EntityId, value); } }
        public bool AllowSleep { get { return ManagedRuntimeContext.Rigidbody3DGetAllowSleep(EntityId); } set { ManagedRuntimeContext.Rigidbody3DSetAllowSleep(EntityId, value); } }
        public bool StartAwake { get { return ManagedRuntimeContext.Rigidbody3DGetStartAwake(EntityId); } set { ManagedRuntimeContext.Rigidbody3DSetStartAwake(EntityId, value); } }
        public bool ContinuousCollision { get { return ManagedRuntimeContext.Rigidbody3DGetContinuousCollision(EntityId); } set { ManagedRuntimeContext.Rigidbody3DSetContinuousCollision(EntityId, value); } }
        public Rigidbody3DConstraints Constraints { get { return (Rigidbody3DConstraints)ManagedRuntimeContext.Rigidbody3DGetConstraints(EntityId); } set { ManagedRuntimeContext.Rigidbody3DSetConstraints(EntityId, (uint)value); } }
        public Vector3 LinearVelocity { get { return ManagedRuntimeContext.Rigidbody3DGetLinearVelocity(EntityId); } set { ManagedRuntimeContext.Rigidbody3DSetLinearVelocity(EntityId, value); } }
        public Vector3 AngularVelocity { get { return ManagedRuntimeContext.Rigidbody3DGetAngularVelocity(EntityId); } set { ManagedRuntimeContext.Rigidbody3DSetAngularVelocity(EntityId, value); } }
        public bool Awake { get { return ManagedRuntimeContext.Rigidbody3DGetAwake(EntityId); } set { ManagedRuntimeContext.Rigidbody3DSetAwake(EntityId, value); } }
        /// <summary>Gets the runtime body handle used by query filters. Zero means no live body.</summary>
        public ulong BodyHandle { get { return ManagedRuntimeContext.Rigidbody3DGetBodyHandle(EntityId); } }

        public PhysicsFilter3D CollisionFilter
        {
            get { return ManagedRuntimeContext.Rigidbody3DGetCollisionFilter(EntityId); }
            set { ManagedRuntimeContext.Rigidbody3DSetCollisionFilter(EntityId, value); }
        }

        public uint Layer
        {
            get { return CollisionFilter.Layer; }
            set { PhysicsFilter3D filter = CollisionFilter; filter.Layer = value; CollisionFilter = filter; }
        }

        public uint CollisionMask
        {
            get { return CollisionFilter.Mask; }
            set { PhysicsFilter3D filter = CollisionFilter; filter.Mask = value; CollisionFilter = filter; }
        }

        public int CollisionGroup
        {
            get { return CollisionFilter.Group; }
            set { PhysicsFilter3D filter = CollisionFilter; filter.Group = value; CollisionFilter = filter; }
        }

        /// <summary>Applies force at the body's center of mass.</summary>
        public void AddForce(Vector3 force, ForceMode3D mode = ForceMode3D.Force)
        {
            ManagedRuntimeContext.Rigidbody3DAddForce(EntityId, force, (int)mode);
        }

        /// <summary>Applies force at a world-space position.</summary>
        public void AddForceAtPosition(Vector3 force, Vector3 worldPosition, ForceMode3D mode = ForceMode3D.Force)
        {
            ManagedRuntimeContext.Rigidbody3DAddForceAt(EntityId, force, worldPosition, (int)mode);
        }

        /// <summary>Applies torque around the body's center of mass.</summary>
        public void AddTorque(Vector3 torque, ForceMode3D mode = ForceMode3D.Force)
        {
            ManagedRuntimeContext.Rigidbody3DAddTorque(EntityId, torque, (int)mode);
        }

        public void WakeUp() { Awake = true; }
        public void Sleep() { Awake = false; }

        [Obsolete("Use BodyType instead.")] public BodyType bodyType { get { return BodyType; } set { BodyType = value; } }
        [Obsolete("Use Mass instead.")] public float mass { get { return Mass; } set { Mass = value; } }
        [Obsolete("Use AutoMass instead.")] public bool autoMass { get { return AutoMass; } set { AutoMass = value; } }
        [Obsolete("Use GravityScale instead.")] public float gravityScale { get { return GravityScale; } set { GravityScale = value; } }
        [Obsolete("Use LinearDamping instead.")] public float linearDamping { get { return LinearDamping; } set { LinearDamping = value; } }
        [Obsolete("Use AngularDamping instead.")] public float angularDamping { get { return AngularDamping; } set { AngularDamping = value; } }
        [Obsolete("Use CenterOfMass instead.")] public Vector3 centerOfMass { get { return CenterOfMass; } set { CenterOfMass = value; } }
        [Obsolete("Use AllowSleep instead.")] public bool allowSleep { get { return AllowSleep; } set { AllowSleep = value; } }
        [Obsolete("Use StartAwake instead.")] public bool startAwake { get { return StartAwake; } set { StartAwake = value; } }
        [Obsolete("Use ContinuousCollision instead.")] public bool continuousCollision { get { return ContinuousCollision; } set { ContinuousCollision = value; } }
        [Obsolete("Use Constraints instead.")] public Rigidbody3DConstraints constraints { get { return Constraints; } set { Constraints = value; } }
        [Obsolete("Use LinearVelocity instead.")] public Vector3 linearVelocity { get { return LinearVelocity; } set { LinearVelocity = value; } }
        [Obsolete("Use AngularVelocity instead.")] public Vector3 angularVelocity { get { return AngularVelocity; } set { AngularVelocity = value; } }
        [Obsolete("Use Awake instead.")] public bool isAwake { get { return Awake; } set { Awake = value; } }
        [Obsolete("Use CollisionFilter instead.")] public PhysicsFilter3D collisionFilter { get { return CollisionFilter; } set { CollisionFilter = value; } }
        [Obsolete("Use Layer instead.")] public uint layer { get { return Layer; } set { Layer = value; } }
        [Obsolete("Use CollisionMask instead.")] public uint collisionMask { get { return CollisionMask; } set { CollisionMask = value; } }
        [Obsolete("Use CollisionGroup instead.")] public int collisionGroup { get { return CollisionGroup; } set { CollisionGroup = value; } }

    }
}

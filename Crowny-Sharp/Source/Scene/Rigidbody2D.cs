using System;

namespace Crowny
{
    public enum ForceMode2D
    {
        Force = 0,
        Impulse = 1
    }

    public enum BodyType
    {
        Static = 0,
        Dynamic = 1,
        Kinematic = 2
    }

    [Flags]
    public enum Rigidbody2DConstraints : uint
    {
        None = 0,
        FreezeRotation = 1,
        FreezePositionX = 2,
        FreezePositionY = 4,
        FreezePosition = FreezePositionX | FreezePositionY,
        FreezeAll = FreezeRotation | FreezePosition
    }

    public enum RigidbodySleepMode
    {
        NeverSleep = 0,
        StartAwake = 1,
        StartSleeping = 2
    }

    public enum CollisionDetectionMode2D
    {
        Discrete = 0,
        Continuous = 1
    }

    public enum RigidbodyInterpolation2D
    {
        None = 0,
        Interpolate = 1,
        Extrapolate = 2
    }

    /// <summary>A 2D rigid body controlled by the active physics backend.</summary>
    public sealed class Rigidbody2D : Component
    {
        private UUID EntityId => entity.uuid;

        public float Mass { get { return ManagedRuntimeContext.Rigidbody2DGetMass(EntityId); } set { ManagedRuntimeContext.Rigidbody2DSetMass(EntityId, value); } }
        public BodyType BodyType { get { return (BodyType)ManagedRuntimeContext.Rigidbody2DGetBodyType(EntityId); } set { ManagedRuntimeContext.Rigidbody2DSetBodyType(EntityId, (int)value); } }
        public RigidbodySleepMode SleepMode { get { return (RigidbodySleepMode)ManagedRuntimeContext.Rigidbody2DGetSleepMode(EntityId); } set { ManagedRuntimeContext.Rigidbody2DSetSleepMode(EntityId, (int)value); } }
        public CollisionDetectionMode2D CollisionDetectionMode
        {
            get { return (CollisionDetectionMode2D)ManagedRuntimeContext.Rigidbody2DGetCollisionDetectionMode(EntityId); }
            set { ManagedRuntimeContext.Rigidbody2DSetCollisionDetectionMode(EntityId, (int)value); }
        }
        public RigidbodyInterpolation2D Interpolation
        {
            get { return (RigidbodyInterpolation2D)ManagedRuntimeContext.Rigidbody2DGetInterpolation(EntityId); }
            set { ManagedRuntimeContext.Rigidbody2DSetInterpolation(EntityId, (int)value); }
        }
        public bool AutoMass { get { return ManagedRuntimeContext.Rigidbody2DGetAutoMass(EntityId); } set { ManagedRuntimeContext.Rigidbody2DSetAutoMass(EntityId, value); } }
        public int Layer
        {
            get { return ManagedRuntimeContext.Rigidbody2DGetLayer(EntityId); }
            set
            {
                if (value < 0 || value >= Physics2D.LayerCount)
                    throw new ArgumentOutOfRangeException("value", "Physics layers must be in the range 0 through 15.");
                ManagedRuntimeContext.Rigidbody2DSetLayer(EntityId, value);
            }
        }
        public float LinearDrag { get { return ManagedRuntimeContext.Rigidbody2DGetLinearDrag(EntityId); } set { ManagedRuntimeContext.Rigidbody2DSetLinearDrag(EntityId, value); } }
        public float AngularDrag { get { return ManagedRuntimeContext.Rigidbody2DGetAngularDrag(EntityId); } set { ManagedRuntimeContext.Rigidbody2DSetAngularDrag(EntityId, value); } }
        public float GravityScale { get { return ManagedRuntimeContext.Rigidbody2DGetGravityScale(EntityId); } set { ManagedRuntimeContext.Rigidbody2DSetGravityScale(EntityId, value); } }
        public Vector2 CenterOfMass { get { return ManagedRuntimeContext.Rigidbody2DGetCenterOfMass(EntityId); } set { ManagedRuntimeContext.Rigidbody2DSetCenterOfMass(EntityId, value); } }
        public float Inertia { get { return ManagedRuntimeContext.Rigidbody2DGetInertia(EntityId); } set { ManagedRuntimeContext.Rigidbody2DSetInertia(EntityId, value); } }
        public Rigidbody2DConstraints Constraints { get { return (Rigidbody2DConstraints)ManagedRuntimeContext.Rigidbody2DGetConstraints(EntityId); } set { ManagedRuntimeContext.Rigidbody2DSetConstraints(EntityId, (uint)value); } }
        public float Rotation => ManagedRuntimeContext.Rigidbody2DGetRotation(EntityId);
        public Vector2 Position => ManagedRuntimeContext.Rigidbody2DGetPosition(EntityId);
        public Vector2 LinearVelocity { get { return ManagedRuntimeContext.Rigidbody2DGetLinearVelocity(EntityId); } set { ManagedRuntimeContext.Rigidbody2DSetLinearVelocity(EntityId, value); } }
        public float AngularVelocity { get { return ManagedRuntimeContext.Rigidbody2DGetAngularVelocity(EntityId); } set { ManagedRuntimeContext.Rigidbody2DSetAngularVelocity(EntityId, value); } }
        public bool Awake { get { return ManagedRuntimeContext.Rigidbody2DGetAwake(EntityId); } set { ManagedRuntimeContext.Rigidbody2DSetAwake(EntityId, value); } }

        public void AddForce(Vector2 force, ForceMode2D mode = ForceMode2D.Force) =>
            ManagedRuntimeContext.Rigidbody2DAddForce(EntityId, force, (int)mode);
        public void AddForceAtPosition(Vector2 force, Vector2 worldPosition, ForceMode2D mode = ForceMode2D.Force) =>
            ManagedRuntimeContext.Rigidbody2DAddForceAtPosition(EntityId, force, worldPosition, (int)mode);
        public void AddTorque(float torque, ForceMode2D mode = ForceMode2D.Force) =>
            ManagedRuntimeContext.Rigidbody2DAddTorque(EntityId, torque, (int)mode);

        public void WakeUp() { Awake = true; }
        public void Sleep() { Awake = false; }
        public bool IsAwake() { return Awake; }
        public bool IsSleeping() { return !Awake; }

        [Obsolete("Use Mass instead.")] public float mass { get { return Mass; } set { Mass = value; } }
        [Obsolete("Use BodyType instead.")] public BodyType bodyType { get { return BodyType; } set { BodyType = value; } }
        [Obsolete("Use SleepMode instead.")] public RigidbodySleepMode sleepMode { get { return SleepMode; } set { SleepMode = value; } }
        [Obsolete("Use CollisionDetectionMode instead.")] public CollisionDetectionMode2D collisionDetectionMode { get { return CollisionDetectionMode; } set { CollisionDetectionMode = value; } }
        [Obsolete("Use AutoMass instead.")] public bool autoMass { get { return AutoMass; } set { AutoMass = value; } }
        [Obsolete("Use Layer instead.")] public int layer { get { return Layer; } set { Layer = value; } }
        [Obsolete("Use LinearDrag instead.")] public float linearDrag { get { return LinearDrag; } set { LinearDrag = value; } }
        [Obsolete("Use AngularDrag instead.")] public float angularDrag { get { return AngularDrag; } set { AngularDrag = value; } }
        [Obsolete("Use GravityScale instead.")] public float gravityScale { get { return GravityScale; } set { GravityScale = value; } }
        [Obsolete("Use CenterOfMass instead.")] public Vector2 centerOfMass { get { return CenterOfMass; } set { CenterOfMass = value; } }
        [Obsolete("Use Inertia instead.")] public float inertia { get { return Inertia; } set { Inertia = value; } }
        [Obsolete("Use Constraints instead.")] public Rigidbody2DConstraints constraints { get { return Constraints; } set { Constraints = value; } }
        [Obsolete("Use Rotation instead.")] public float rotation => Rotation;
        [Obsolete("Use Position instead.")] public Vector2 position => Position;
    }
}

using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    /// <summary>Controls how a force changes a 2D body.</summary>
    public enum ForceMode2D
    {
        Force = 0,
        Impulse = 1
    }

    /// <summary>Controls how a rigid body participates in simulation.</summary>
    public enum BodyType
    {
        Static = 0,
        Dynamic = 1,
        Kinematic = 2
    }

    /// <summary>Locks selected degrees of freedom on a 2D body.</summary>
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
#if CROWNY_MONO
        public float Mass { get { return Internal_GetMass(m_InternalPtr); } set { Internal_SetMass(m_InternalPtr, value); } }
        public BodyType BodyType { get { return Internal_GetBodyType(m_InternalPtr); } set { Internal_SetBodyType(m_InternalPtr, value); } }
        public RigidbodySleepMode SleepMode { get { return Internal_GetSleepMode(m_InternalPtr); } set { Internal_SetSleepMode(m_InternalPtr, value); } }
        public CollisionDetectionMode2D CollisionDetectionMode { get { return Internal_GetCollisionDetectionMode(m_InternalPtr); } set { Internal_SetCollisionDetectionMode(m_InternalPtr, value); } }
        public RigidbodyInterpolation2D Interpolation { get { return Internal_GetInterpolationMode(m_InternalPtr); } set { Internal_SetInterpolationMode(m_InternalPtr, value); } }
        public bool AutoMass { get { return Internal_GetAutoMass(m_InternalPtr); } set { Internal_SetAutoMass(m_InternalPtr, value); } }
        public int Layer
        {
            get { return Internal_GetLayer(m_InternalPtr); }
            set
            {
                if (value < 0 || value >= Physics2D.LayerCount)
                    throw new ArgumentOutOfRangeException("value", "Physics layers must be in the range 0 through 15.");
                Internal_SetLayer(m_InternalPtr, value);
            }
        }
        public float LinearDrag { get { return Internal_GetLinearDrag(m_InternalPtr); } set { Internal_SetLinearDrag(m_InternalPtr, value); } }
        public float AngularDrag { get { return Internal_GetAngularDrag(m_InternalPtr); } set { Internal_SetAngularDrag(m_InternalPtr, value); } }
        public float GravityScale { get { return Internal_GetGravityScale(m_InternalPtr); } set { Internal_SetGravityScale(m_InternalPtr, value); } }
        public Vector2 CenterOfMass { get { Internal_GetCenterOfMass(m_InternalPtr, out Vector2 value); return value; } set { Internal_SetCenterOfMass(m_InternalPtr, ref value); } }
        public float Inertia { get { return Internal_GetInertia(m_InternalPtr); } set { Internal_SetInertia(m_InternalPtr, value); } }
        public Rigidbody2DConstraints Constraints { get { return Internal_GetConstraints(m_InternalPtr); } set { Internal_SetConstraints(m_InternalPtr, value); } }
        public float Rotation { get { return Internal_GetRotation(m_InternalPtr); } }
        public Vector2 Position { get { Internal_GetPosition(m_InternalPtr, out Vector2 value); return value; } }
        public Vector2 LinearVelocity { get { Internal_GetLinearVelocity(m_InternalPtr, out Vector2 value); return value; } set { Internal_SetLinearVelocity(m_InternalPtr, ref value); } }
        public float AngularVelocity { get { return Internal_GetAngularVelocity(m_InternalPtr); } set { Internal_SetAngularVelocity(m_InternalPtr, value); } }
        public bool Awake { get { return Internal_IsAwake(m_InternalPtr); } set { Internal_SetAwake(m_InternalPtr, value); } }

        /// <summary>Applies force at the body's center of mass.</summary>
        public void AddForce(Vector2 force, ForceMode2D mode = ForceMode2D.Force)
        {
            Internal_AddForce(m_InternalPtr, ref force, mode);
        }

        /// <summary>Applies force at a world-space position.</summary>
        public void AddForceAtPosition(Vector2 force, Vector2 worldPosition, ForceMode2D mode = ForceMode2D.Force)
        {
            Internal_AddForceAt(m_InternalPtr, ref force, ref worldPosition, mode);
        }

        /// <summary>Applies torque around the body's center of mass.</summary>
        public void AddTorque(float torque, ForceMode2D mode = ForceMode2D.Force)
        {
            Internal_AddTorque(m_InternalPtr, torque, mode);
        }
#else
        private UUID EntityId => entity.uuid;

        public float Mass
        {
            get { return ManagedRuntimeContext.Rigidbody2DGetMass(EntityId); }
            set { ManagedRuntimeContext.Rigidbody2DSetMass(EntityId, value); }
        }

        public BodyType BodyType
        {
            get { return (BodyType)ManagedRuntimeContext.Rigidbody2DGetBodyType(EntityId); }
            set { ManagedRuntimeContext.Rigidbody2DSetBodyType(EntityId, (int)value); }
        }

        public RigidbodySleepMode SleepMode
        {
            get { return (RigidbodySleepMode)ManagedRuntimeContext.Rigidbody2DGetSleepMode(EntityId); }
            set { ManagedRuntimeContext.Rigidbody2DSetSleepMode(EntityId, (int)value); }
        }

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

        public bool AutoMass
        {
            get { return ManagedRuntimeContext.Rigidbody2DGetAutoMass(EntityId); }
            set { ManagedRuntimeContext.Rigidbody2DSetAutoMass(EntityId, value); }
        }

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

        public float LinearDrag
        {
            get { return ManagedRuntimeContext.Rigidbody2DGetLinearDrag(EntityId); }
            set { ManagedRuntimeContext.Rigidbody2DSetLinearDrag(EntityId, value); }
        }

        public float AngularDrag
        {
            get { return ManagedRuntimeContext.Rigidbody2DGetAngularDrag(EntityId); }
            set { ManagedRuntimeContext.Rigidbody2DSetAngularDrag(EntityId, value); }
        }

        public float GravityScale
        {
            get { return ManagedRuntimeContext.Rigidbody2DGetGravityScale(EntityId); }
            set { ManagedRuntimeContext.Rigidbody2DSetGravityScale(EntityId, value); }
        }

        public Vector2 CenterOfMass
        {
            get { return ManagedRuntimeContext.Rigidbody2DGetCenterOfMass(EntityId); }
            set { ManagedRuntimeContext.Rigidbody2DSetCenterOfMass(EntityId, value); }
        }

        public float Inertia
        {
            get { return ManagedRuntimeContext.Rigidbody2DGetInertia(EntityId); }
            set { ManagedRuntimeContext.Rigidbody2DSetInertia(EntityId, value); }
        }

        public Rigidbody2DConstraints Constraints
        {
            get { return (Rigidbody2DConstraints)ManagedRuntimeContext.Rigidbody2DGetConstraints(EntityId); }
            set { ManagedRuntimeContext.Rigidbody2DSetConstraints(EntityId, (uint)value); }
        }

        public float Rotation => ManagedRuntimeContext.Rigidbody2DGetRotation(EntityId);
        public Vector2 Position => ManagedRuntimeContext.Rigidbody2DGetPosition(EntityId);

        public Vector2 LinearVelocity
        {
            get { return ManagedRuntimeContext.Rigidbody2DGetLinearVelocity(EntityId); }
            set { ManagedRuntimeContext.Rigidbody2DSetLinearVelocity(EntityId, value); }
        }

        public float AngularVelocity
        {
            get { return ManagedRuntimeContext.Rigidbody2DGetAngularVelocity(EntityId); }
            set { ManagedRuntimeContext.Rigidbody2DSetAngularVelocity(EntityId, value); }
        }

        public bool Awake
        {
            get { return ManagedRuntimeContext.Rigidbody2DGetAwake(EntityId); }
            set { ManagedRuntimeContext.Rigidbody2DSetAwake(EntityId, value); }
        }

        /// <summary>Applies force at the body's center of mass.</summary>
        public void AddForce(Vector2 force, ForceMode2D mode = ForceMode2D.Force)
        {
            ManagedRuntimeContext.Rigidbody2DAddForce(EntityId, force, (int)mode);
        }

        /// <summary>Applies force at a world-space position.</summary>
        public void AddForceAtPosition(Vector2 force, Vector2 worldPosition, ForceMode2D mode = ForceMode2D.Force)
        {
            ManagedRuntimeContext.Rigidbody2DAddForceAtPosition(EntityId, force, worldPosition, (int)mode);
        }

        /// <summary>Applies torque around the body's center of mass.</summary>
        public void AddTorque(float torque, ForceMode2D mode = ForceMode2D.Force)
        {
            ManagedRuntimeContext.Rigidbody2DAddTorque(EntityId, torque, (int)mode);
        }
#endif

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
        [Obsolete("Use Rotation instead.")] public float rotation { get { return Rotation; } }
        [Obsolete("Use Position instead.")] public Vector2 position { get { return Position; } }

#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern bool Internal_IsAwake(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetAwake(IntPtr body, bool value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern BodyType Internal_GetBodyType(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern int Internal_GetLayer(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetLinearDrag(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetAngularDrag(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetGravityScale(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern Rigidbody2DConstraints Internal_GetConstraints(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetMass(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern bool Internal_GetAutoMass(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_GetCenterOfMass(IntPtr body, out Vector2 value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern RigidbodySleepMode Internal_GetSleepMode(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern CollisionDetectionMode2D Internal_GetCollisionDetectionMode(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern RigidbodyInterpolation2D Internal_GetInterpolationMode(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetMass(IntPtr body, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetAutoMass(IntPtr body, bool value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetCenterOfMass(IntPtr body, ref Vector2 value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetGravityScale(IntPtr body, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetBodyType(IntPtr body, BodyType value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetLayer(IntPtr body, int value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetConstraints(IntPtr body, Rigidbody2DConstraints value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetLinearDrag(IntPtr body, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetAngularDrag(IntPtr body, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetSleepMode(IntPtr body, RigidbodySleepMode value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetCollisionDetectionMode(IntPtr body, CollisionDetectionMode2D value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetInterpolationMode(IntPtr body, RigidbodyInterpolation2D value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_AddForce(IntPtr body, ref Vector2 force, ForceMode2D mode);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_AddForceAt(IntPtr body, ref Vector2 force, ref Vector2 worldPosition, ForceMode2D mode);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_AddTorque(IntPtr body, float torque, ForceMode2D mode);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_GetPosition(IntPtr body, out Vector2 value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetRotation(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetInertia(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetInertia(IntPtr body, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_GetLinearVelocity(IntPtr body, out Vector2 value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetLinearVelocity(IntPtr body, ref Vector2 value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetAngularVelocity(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetAngularVelocity(IntPtr body, float value);
#endif
    }
}

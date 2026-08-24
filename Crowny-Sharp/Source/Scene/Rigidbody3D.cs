using System;
using System.Runtime.CompilerServices;

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
        public BodyType BodyType { get { return Internal_GetBodyType(m_InternalPtr); } set { Internal_SetBodyType(m_InternalPtr, value); } }
        public float Mass { get { return Internal_GetMass(m_InternalPtr); } set { Internal_SetMass(m_InternalPtr, value); } }
        public bool AutoMass { get { return Internal_GetAutoMass(m_InternalPtr); } set { Internal_SetAutoMass(m_InternalPtr, value); } }
        public float GravityScale { get { return Internal_GetGravityScale(m_InternalPtr); } set { Internal_SetGravityScale(m_InternalPtr, value); } }
        public float LinearDamping { get { return Internal_GetLinearDamping(m_InternalPtr); } set { Internal_SetLinearDamping(m_InternalPtr, value); } }
        public float AngularDamping { get { return Internal_GetAngularDamping(m_InternalPtr); } set { Internal_SetAngularDamping(m_InternalPtr, value); } }
        public Vector3 CenterOfMass { get { Internal_GetCenterOfMass(m_InternalPtr, out Vector3 value); return value; } set { Internal_SetCenterOfMass(m_InternalPtr, ref value); } }
        public bool AllowSleep { get { return Internal_GetAllowSleep(m_InternalPtr); } set { Internal_SetAllowSleep(m_InternalPtr, value); } }
        public bool StartAwake { get { return Internal_GetStartAwake(m_InternalPtr); } set { Internal_SetStartAwake(m_InternalPtr, value); } }
        public bool ContinuousCollision { get { return Internal_GetContinuousCollision(m_InternalPtr); } set { Internal_SetContinuousCollision(m_InternalPtr, value); } }
        public Rigidbody3DConstraints Constraints { get { return Internal_GetConstraints(m_InternalPtr); } set { Internal_SetConstraints(m_InternalPtr, value); } }
        public Vector3 LinearVelocity { get { Internal_GetLinearVelocity(m_InternalPtr, out Vector3 value); return value; } set { Internal_SetLinearVelocity(m_InternalPtr, ref value); } }
        public Vector3 AngularVelocity { get { Internal_GetAngularVelocity(m_InternalPtr, out Vector3 value); return value; } set { Internal_SetAngularVelocity(m_InternalPtr, ref value); } }
        public bool Awake { get { return Internal_IsAwake(m_InternalPtr); } set { Internal_SetAwake(m_InternalPtr, value); } }
        /// <summary>Gets the runtime body handle used by query filters. Zero means no live body.</summary>
        public ulong BodyHandle { get { return Internal_GetBodyHandle(m_InternalPtr); } }

        public PhysicsFilter3D CollisionFilter
        {
            get { Internal_GetFilter(m_InternalPtr, out PhysicsFilter3D value); return value; }
            set { Internal_SetFilter(m_InternalPtr, ref value); }
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
            Internal_AddForce(m_InternalPtr, ref force, mode);
        }

        /// <summary>Applies force at a world-space position.</summary>
        public void AddForceAtPosition(Vector3 force, Vector3 worldPosition, ForceMode3D mode = ForceMode3D.Force)
        {
            Internal_AddForceAt(m_InternalPtr, ref force, ref worldPosition, mode);
        }

        /// <summary>Applies torque around the body's center of mass.</summary>
        public void AddTorque(Vector3 torque, ForceMode3D mode = ForceMode3D.Force)
        {
            Internal_AddTorque(m_InternalPtr, ref torque, mode);
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

        [MethodImpl(MethodImplOptions.InternalCall)] private static extern BodyType Internal_GetBodyType(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetBodyType(IntPtr body, BodyType value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetMass(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetMass(IntPtr body, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern bool Internal_GetAutoMass(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetAutoMass(IntPtr body, bool value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetGravityScale(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetGravityScale(IntPtr body, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetLinearDamping(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetLinearDamping(IntPtr body, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetAngularDamping(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetAngularDamping(IntPtr body, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_GetCenterOfMass(IntPtr body, out Vector3 value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetCenterOfMass(IntPtr body, ref Vector3 value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern bool Internal_GetAllowSleep(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetAllowSleep(IntPtr body, bool value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern bool Internal_GetStartAwake(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetStartAwake(IntPtr body, bool value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern bool Internal_GetContinuousCollision(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetContinuousCollision(IntPtr body, bool value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern Rigidbody3DConstraints Internal_GetConstraints(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetConstraints(IntPtr body, Rigidbody3DConstraints value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_GetFilter(IntPtr body, out PhysicsFilter3D value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetFilter(IntPtr body, ref PhysicsFilter3D value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_GetLinearVelocity(IntPtr body, out Vector3 value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetLinearVelocity(IntPtr body, ref Vector3 value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_GetAngularVelocity(IntPtr body, out Vector3 value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetAngularVelocity(IntPtr body, ref Vector3 value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern bool Internal_IsAwake(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetAwake(IntPtr body, bool value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern ulong Internal_GetBodyHandle(IntPtr body);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_AddForce(IntPtr body, ref Vector3 force, ForceMode3D mode);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_AddForceAt(IntPtr body, ref Vector3 force, ref Vector3 worldPosition, ForceMode3D mode);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_AddTorque(IntPtr body, ref Vector3 torque, ForceMode3D mode);
    }
}

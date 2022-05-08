using System;
using System.Runtime.CompilerServices;

namespace Crowny
{

    public enum ForceMode2D
    {
        Force,
        Impulse
    }

    public enum BodyType
    {
        Static,
        Dynamic,
        Kinematic
    }

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
    };

    public enum CollisionDetectionMode2D
    {
        Discrete = 0,
        Continuous = 1
    };

    public class Rigidbody2D : Component
    {
        public float mass { get { return Internal_GetMass(m_InternalPtr); } }
        public BodyType bodyType { get { return Internal_GetBodyType(m_InternalPtr); } set { Internal_SetBodyType(m_InternalPtr, value); } }

        public RigidbodySleepMode sleepMode { get { return Internal_GetSleepMode(m_InternalPtr); } set { Internal_SetSleepMode(m_InternalPtr, value); } }
        public CollisionDetectionMode2D collisionDetectionMode { get { return Internal_GetCollisionDetectionMode(m_InternalPtr); } set { Internal_SetCollisionDetectionMode(m_InternalPtr, value); } }
        public bool autoMass { get { return Internal_GetAutoMass(m_InternalPtr); } set { Internal_SetAutoMass(m_InternalPtr, value); } }
        public int layer { get { return Internal_GetLayer(m_InternalPtr); } set { Internal_SetLayer(m_InternalPtr, value); } }
        public float linearDrag { get { return Internal_GetLinearDrag(m_InternalPtr); } set { Internal_SetLinearDrag(m_InternalPtr, value); } }
        public float angularDrag { get { return Internal_GetAngularDrag(m_InternalPtr); } set { Internal_SetAngularDrag(m_InternalPtr, value); } }
        public Vector2 centerOfMass { get { Internal_GetCenterOfMass(m_InternalPtr, out Vector2 center); return center; } set { Internal_SetCenterOfMass(m_InternalPtr, ref value); } }

        public Rigidbody2DConstraints constraints { get { return Internal_GetConstraints(m_InternalPtr, m_InternalPtr); } set { Internal_SetConstraints(m_InternalPtr, value);  } }

        public float rotation { get { return Internal_GetRotation(m_InternalPtr); } }

        public void AddForce(Vector2 force, ForceMode2D forceMode = ForceMode2D.Force)
        {
            Internal_AddForce(m_InternalPtr, ref force, forceMode);
        }

        public void AddForceAtPosition(Vector2 force, Vector2 offset, ForceMode2D forceMode = ForceMode2D.Force)
        {
            Internal_AddForceAt(m_InternalPtr, ref force, ref offset, forceMode);
        }

        public void AddTorque(Vector2 torque, ForceMode2D forceMode = ForceMode2D.Force)
        {
            Internal_AddTorque(m_InternalPtr, ref torque, forceMode);
        }

        public bool IsAwake() => Internal_IsAwake(m_InternalPtr);

        public bool IsSleeping() => !IsAwake();


        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_IsAwake(IntPtr thisPtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern BodyType Internal_GetBodyType(IntPtr parent);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int Internal_GetLayer(IntPtr parent);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetLinearDrag(IntPtr parent);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetAngularDrag(IntPtr parent);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Rigidbody2DConstraints Internal_GetConstraints(IntPtr parent, IntPtr parent2);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetMass(IntPtr parent);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_GetAutoMass(IntPtr parent);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetCenterOfMass(IntPtr parent, out Vector2 centerOfMass);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern RigidbodySleepMode Internal_GetSleepMode(IntPtr parent);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern CollisionDetectionMode2D Internal_GetCollisionDetectionMode(IntPtr parent);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetMass(IntPtr parent, float mass);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetAutoMass(IntPtr parent, bool autoMass);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetCenterOfMass(IntPtr parent, ref Vector2 centerOfMass);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetGravityScale(IntPtr parent, float gravityScale);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetBodyType(IntPtr parent, BodyType bodyType);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetLayer(IntPtr parent, int layer);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetConstraints(IntPtr parent, Rigidbody2DConstraints constraints);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetLinearDrag(IntPtr parent, float linearDrag);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetAngularDrag(IntPtr parent, float angularDrag);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetSleepMode(IntPtr parent, RigidbodySleepMode sleepMode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetCollisionDetectionMode(IntPtr parent, CollisionDetectionMode2D collisionDetection);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_AddForce(IntPtr parent, ref Vector2 force, ForceMode2D forceMode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_AddForceAt(IntPtr parent, ref Vector2 offset, ref Vector2 force, ForceMode2D forceMode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_AddTorque(IntPtr parent, ref Vector2 troque, ForceMode2D forceMode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetPosition(IntPtr parent, ref Vector2 position);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetRotation(IntPtr parent);
    }
}

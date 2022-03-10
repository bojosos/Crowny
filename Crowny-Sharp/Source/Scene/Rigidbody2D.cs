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

    public enum Rigidbody2DConstraints
    {
        None = 0,
        FreezeRotation = 1,
        FreezePositionX = 2,
        FreezePositionY = 4,
        FreezePosition = FreezePositionX | FreezePositionY,
        FreezeAll = FreezeRotation | FreezePosition
    }

    public class Rigidbody2D : Component
    {

        public float mass { get { return Internal_GetMass(m_InternalPtr); } }
        public BodyType bodyType { get { return Internal_GetBodyType(m_InternalPtr); } }
        public float rotation { get { return Internal_GetRotation(m_InternalPtr); } }

        public Rigidbody2DConstraints constraints { get { return Internal_GetConstraints(m_InternalPtr); } }
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

        public bool IsAwake()
        {
            return true;
        }

        public bool IsSleeping() { return !IsAwake(); }

        public void MovePosition(Vector2 position) { }

        public void MoveRotation(float angle) { }

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern BodyType Internal_GetBodyType(IntPtr parent);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern float Internal_GetRotation(IntPtr parent);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern Rigidbody2DConstraints Internal_GetConstraints(IntPtr parent);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern float Internal_GetMass(IntPtr parent);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_AddForce(IntPtr parent, ref Vector2 force, ForceMode2D forceMode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_AddForceAt(IntPtr parent, ref Vector2 offset, ref Vector2 force, ForceMode2D forceMode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_AddTorque(IntPtr parent, ref Vector2 troque, ForceMode2D forceMode);
    }
}

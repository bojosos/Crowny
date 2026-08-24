using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Crowny
{
    /// <summary>One contact reported for a 2D collision.</summary>
    public struct ContactPoint2D
    {
        public Vector2 Point;

        internal ContactPoint2D(Vector2 point)
        {
            Point = point;
        }
    }

    /// <summary>Contact data passed to a 2D collision callback.</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct Collision2D
    {
        public Entity[] Colliders;
        public Vector2[] Points;

        /// <summary>Gets the number of reported contacts.</summary>
        public int ContactCount { get { return Points == null ? 0 : Points.Length; } }

        /// <summary>Gets a contact by index without allocating another array.</summary>
        public ContactPoint2D GetContact(int index)
        {
            if (Points == null)
                throw new IndexOutOfRangeException("The collision has no contact points.");
            return new ContactPoint2D(Points[index]);
        }

        [Obsolete("Use Colliders instead.")]
        public Entity[] colliders { get { return Colliders; } }

        [Obsolete("Use Points instead.")]
        public Vector2[] points { get { return Points; } }
    }

    /// <summary>Base class for 2D collision shapes.</summary>
    public class Collider2D : Component
    {
        public bool IsTrigger
        {
            get { return Internal_IsTrigger(m_InternalPtr); }
            set { Internal_SetTrigger(m_InternalPtr, value); }
        }

        public Vector2 Offset
        {
            get { Internal_GetOffset(m_InternalPtr, out Vector2 value); return value; }
            set { Internal_SetOffset(m_InternalPtr, ref value); }
        }

        public PhysicsMaterial2D Material
        {
            get { return Internal_GetMaterial(m_InternalPtr); }
            set { Internal_SetMaterial(m_InternalPtr, value); }
        }

        [Obsolete("Use IsTrigger instead.")]
        public bool isTrigger { get { return IsTrigger; } set { IsTrigger = value; } }

        [Obsolete("Use Offset instead.")]
        public Vector2 offset { get { return Offset; } set { Offset = value; } }

        [MethodImpl(MethodImplOptions.InternalCall)] private static extern bool Internal_IsTrigger(IntPtr collider);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetTrigger(IntPtr collider, bool trigger);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_GetOffset(IntPtr collider, out Vector2 offset);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetOffset(IntPtr collider, ref Vector2 offset);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern PhysicsMaterial2D Internal_GetMaterial(IntPtr collider);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetMaterial(IntPtr collider, PhysicsMaterial2D material);
    }

    /// <summary>A rectangular 2D collision shape.</summary>
    public sealed class BoxCollider2D : Collider2D
    {
        public Vector2 Size
        {
            get { Internal_GetSize(m_InternalPtr, out Vector2 value); return value; }
            set { Internal_SetSize(m_InternalPtr, ref value); }
        }

        [Obsolete("Use Size instead.")]
        public Vector2 size { get { return Size; } set { Size = value; } }

        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_GetSize(IntPtr collider, out Vector2 size);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetSize(IntPtr collider, ref Vector2 size);
    }

    /// <summary>A circular 2D collision shape.</summary>
    public sealed class CircleCollider2D : Collider2D
    {
        public float Radius
        {
            get { return Internal_GetRadius(m_InternalPtr); }
            set { Internal_SetRadius(m_InternalPtr, value); }
        }

        [Obsolete("Use Radius instead.")]
        public float radius { get { return Radius; } set { Radius = value; } }

        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetRadius(IntPtr collider);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetRadius(IntPtr collider, float radius);
    }
}

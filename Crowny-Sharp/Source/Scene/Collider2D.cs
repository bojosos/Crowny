using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Crowny
{

    [StructLayout(LayoutKind.Sequential)]
    public struct Collision2D
    {
        public Entity[] colliders;
        public Vector2[] points;
    }

    public class PhysicsMaterial2D // This will be an asset later
    {
#pragma warning disable 0414
        float Density = 1.0f;
        float Friction = 0.5f;
        float Restitution = 0.0f;
        float RestitutionThreshold = 0.5f;
#pragma warning restore 0414
    }

    internal enum ColliderType : int {
        Box = 0,
        Circle = 1,
        Edge = 2,
        Polygon = 3
    }


    public class Collider2D : Component
    {
        public virtual bool isTrigger { get; set; }
        public virtual Vector2 offset { get; set; }
        public virtual PhysicsMaterial2D material { get; set; }
        // public Bounds bounds { get; }
    }

    public class BoxCollider2D : Collider2D
    {
        public Vector2 size { get { Internal_GetSize(m_InternalPtr, out Vector2 size); return size; } set { Internal_SetSize(m_InternalPtr, ref value); } }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetSize(IntPtr parent, out Vector2 offset);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetSize(IntPtr parent, ref Vector2 size);

        public override bool isTrigger { get { return Internal_IsTrigger(m_InternalPtr); } set { Internal_SetTrigger(m_InternalPtr, value); } }
        public override Vector2 offset { get { Internal_GetOffset(m_InternalPtr, out Vector2 offset); return offset; } set { Internal_SetOffset(m_InternalPtr, ref value); } }
        public override PhysicsMaterial2D material { get { return null; } set { } }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_IsTrigger(IntPtr parent);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetTrigger(IntPtr parent, bool trigger);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetOffset(IntPtr parent, out Vector2 offset);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetOffset(IntPtr parent, ref Vector2 offset);
    }
    
    public class CircleCollider2D : Collider2D
    {
        public float radius { get { return Internal_GetRadius(m_InternalPtr); } set { Internal_SetRadius(m_InternalPtr, value); } }

        public override bool isTrigger { get { return Internal_IsTrigger(m_InternalPtr); } set { Internal_SetTrigger(m_InternalPtr, value); } }
        public override Vector2 offset { get { Internal_GetOffset(m_InternalPtr, out Vector2 offset); return offset; } set { Internal_SetOffset(m_InternalPtr, ref value); } }
        public override PhysicsMaterial2D material { get { return null; } set { } }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetRadius(IntPtr parent);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetRadius(IntPtr parent, float radius);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_IsTrigger(IntPtr parent);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetTrigger(IntPtr parent, bool trigger);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetOffset(IntPtr parent, out Vector2 offset);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetOffset(IntPtr parent, ref Vector2 offset);
    }

}

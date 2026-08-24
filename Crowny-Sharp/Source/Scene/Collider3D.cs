using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Crowny
{
    /// <summary>One contact reported for a 3D collision.</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct ContactPoint3D
    {
        public Vector3 Point;
        public Vector3 Normal;
        public float Separation;
        public float NormalImpulse;

        internal ContactPoint3D(Vector3 point, Vector3 normal, float separation, float normalImpulse)
        {
            Point = point;
            Normal = normal;
            Separation = separation;
            NormalImpulse = normalImpulse;
        }
    }

    /// <summary>Contact data passed to a 3D collision callback.</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct Collision3D
    {
        public Entity[] Colliders;
        public ContactPoint3D[] Contacts;

        /// <summary>Gets the number of reported contacts.</summary>
        public int ContactCount { get { return Contacts == null ? 0 : Contacts.Length; } }

        /// <summary>Gets a contact by index without allocating another array.</summary>
        public ContactPoint3D GetContact(int index)
        {
            if (Contacts == null)
                throw new IndexOutOfRangeException("The collision has no contact points.");
            return Contacts[index];
        }

        [Obsolete("Use Colliders instead.")]
        public Entity[] colliders { get { return Colliders; } }

        /// <summary>Creates an array containing only contact positions.</summary>
        [Obsolete("Use Contacts instead. This compatibility property allocates an array.")]
        public Vector3[] contactPoints
        {
            get
            {
                int count = ContactCount;
                Vector3[] values = new Vector3[count];
                for (int i = 0; i < count; ++i)
                    values[i] = Contacts[i].Point;
                return values;
            }
        }

        /// <summary>Creates an array containing only contact normals.</summary>
        [Obsolete("Use Contacts instead. This compatibility property allocates an array.")]
        public Vector3[] contactNormals
        {
            get
            {
                int count = ContactCount;
                Vector3[] values = new Vector3[count];
                for (int i = 0; i < count; ++i)
                    values[i] = Contacts[i].Normal;
                return values;
            }
        }

        /// <summary>Creates an array containing only normal impulses.</summary>
        [Obsolete("Use Contacts instead. This compatibility property allocates an array.")]
        public float[] normalImpulses
        {
            get
            {
                int count = ContactCount;
                float[] values = new float[count];
                for (int i = 0; i < count; ++i)
                    values[i] = Contacts[i].NormalImpulse;
                return values;
            }
        }
    }

    /// <summary>Base class for 3D collision shapes.</summary>
    public class Collider3D : Component
    {
        public bool IsTrigger
        {
            get { return Internal_IsTrigger(m_InternalPtr); }
            set { Internal_SetTrigger(m_InternalPtr, value); }
        }

        public Vector3 Offset
        {
            get { Internal_GetOffset(m_InternalPtr, out Vector3 value); return value; }
            set { Internal_SetOffset(m_InternalPtr, ref value); }
        }

        public Quaternion Rotation
        {
            get { Internal_GetRotation(m_InternalPtr, out Quaternion value); return value; }
            set { Internal_SetRotation(m_InternalPtr, ref value); }
        }

        public PhysicsMaterial3D Material
        {
            get { return Internal_GetMaterial(m_InternalPtr); }
            set { Internal_SetMaterial(m_InternalPtr, value); }
        }

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

        [Obsolete("Use IsTrigger instead.")]
        public bool isTrigger { get { return IsTrigger; } set { IsTrigger = value; } }
        [Obsolete("Use Offset instead.")]
        public Vector3 offset { get { return Offset; } set { Offset = value; } }
        [Obsolete("Use Rotation instead.")]
        public Quaternion rotation { get { return Rotation; } set { Rotation = value; } }
        [Obsolete("Use Material instead.")]
        public PhysicsMaterial3D material { get { return Material; } set { Material = value; } }
        [Obsolete("Use CollisionFilter instead.")]
        public PhysicsFilter3D collisionFilter { get { return CollisionFilter; } set { CollisionFilter = value; } }
        [Obsolete("Use Layer instead.")]
        public uint layer { get { return Layer; } set { Layer = value; } }
        [Obsolete("Use CollisionMask instead.")]
        public uint collisionMask { get { return CollisionMask; } set { CollisionMask = value; } }
        [Obsolete("Use CollisionGroup instead.")]
        public int collisionGroup { get { return CollisionGroup; } set { CollisionGroup = value; } }

        [MethodImpl(MethodImplOptions.InternalCall)] private static extern bool Internal_IsTrigger(IntPtr collider);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetTrigger(IntPtr collider, bool value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_GetOffset(IntPtr collider, out Vector3 value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetOffset(IntPtr collider, ref Vector3 value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_GetRotation(IntPtr collider, out Quaternion value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetRotation(IntPtr collider, ref Quaternion value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern PhysicsMaterial3D Internal_GetMaterial(IntPtr collider);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetMaterial(IntPtr collider, PhysicsMaterial3D value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_GetFilter(IntPtr collider, out PhysicsFilter3D value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetFilter(IntPtr collider, ref PhysicsFilter3D value);
    }

    /// <summary>A box-shaped 3D collider.</summary>
    public sealed class BoxCollider3D : Collider3D
    {
        public Vector3 Size
        {
            get { Internal_GetSize(m_InternalPtr, out Vector3 value); return value; }
            set { Internal_SetSize(m_InternalPtr, ref value); }
        }

        [Obsolete("Use Size instead.")]
        public Vector3 size { get { return Size; } set { Size = value; } }

        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_GetSize(IntPtr collider, out Vector3 value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetSize(IntPtr collider, ref Vector3 value);
    }

    /// <summary>A sphere-shaped 3D collider.</summary>
    public sealed class SphereCollider3D : Collider3D
    {
        public float Radius
        {
            get { return Internal_GetRadius(m_InternalPtr); }
            set { Internal_SetRadius(m_InternalPtr, value); }
        }

        [Obsolete("Use Radius instead.")]
        public float radius { get { return Radius; } set { Radius = value; } }

        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetRadius(IntPtr collider);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetRadius(IntPtr collider, float value);
    }

    /// <summary>A capsule-shaped 3D collider.</summary>
    public sealed class CapsuleCollider3D : Collider3D
    {
        public float Radius
        {
            get { return Internal_GetRadius(m_InternalPtr); }
            set { Internal_SetRadius(m_InternalPtr, value); }
        }

        public float Height
        {
            get { return Internal_GetHeight(m_InternalPtr); }
            set { Internal_SetHeight(m_InternalPtr, value); }
        }

        [Obsolete("Use Radius instead.")]
        public float radius { get { return Radius; } set { Radius = value; } }
        [Obsolete("Use Height instead.")]
        public float height { get { return Height; } set { Height = value; } }

        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetRadius(IntPtr collider);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetRadius(IntPtr collider, float value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern float Internal_GetHeight(IntPtr collider);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetHeight(IntPtr collider, float value);
    }
}

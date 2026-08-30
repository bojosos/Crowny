using System;
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
            get { return ManagedRuntimeContext.Collider3DGetIsTrigger(EntityId); }
            set { ManagedRuntimeContext.Collider3DSetIsTrigger(EntityId, value); }
        }

        public Vector3 Offset
        {
            get { return ManagedRuntimeContext.Collider3DGetOffset(EntityId); }
            set { ManagedRuntimeContext.Collider3DSetOffset(EntityId, value); }
        }

        public Quaternion Rotation
        {
            get { return ManagedRuntimeContext.Collider3DGetRotation(EntityId); }
            set { ManagedRuntimeContext.Collider3DSetRotation(EntityId, value); }
        }

        public PhysicsMaterial3D Material
        {
            get { return ManagedRuntimeContext.CreateAsset<PhysicsMaterial3D>(ManagedRuntimeContext.Collider3DGetMaterial(EntityId)); }
            set { ManagedRuntimeContext.Collider3DSetMaterial(EntityId, value != null ? value.uuid : UUID.Empty); }
        }

        public PhysicsFilter3D CollisionFilter
        {
            get { return ManagedRuntimeContext.Collider3DGetCollisionFilter(EntityId); }
            set { ManagedRuntimeContext.Collider3DSetCollisionFilter(EntityId, value); }
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

    }

    /// <summary>A box-shaped 3D collider.</summary>
    public sealed class BoxCollider3D : Collider3D
    {
        public Vector3 Size
        {
            get { return ManagedRuntimeContext.BoxCollider3DGetSize(EntityId); }
            set { ManagedRuntimeContext.BoxCollider3DSetSize(EntityId, value); }
        }

        [Obsolete("Use Size instead.")]
        public Vector3 size { get { return Size; } set { Size = value; } }

    }

    /// <summary>A sphere-shaped 3D collider.</summary>
    public sealed class SphereCollider3D : Collider3D
    {
        public float Radius
        {
            get { return ManagedRuntimeContext.SphereCollider3DGetRadius(EntityId); }
            set { ManagedRuntimeContext.SphereCollider3DSetRadius(EntityId, value); }
        }

        [Obsolete("Use Radius instead.")]
        public float radius { get { return Radius; } set { Radius = value; } }

    }

    /// <summary>A capsule-shaped 3D collider.</summary>
    public sealed class CapsuleCollider3D : Collider3D
    {
        public float Radius
        {
            get { return ManagedRuntimeContext.CapsuleCollider3DGetRadius(EntityId); }
            set { ManagedRuntimeContext.CapsuleCollider3DSetRadius(EntityId, value); }
        }

        public float Height
        {
            get { return ManagedRuntimeContext.CapsuleCollider3DGetHeight(EntityId); }
            set { ManagedRuntimeContext.CapsuleCollider3DSetHeight(EntityId, value); }
        }

        [Obsolete("Use Radius instead.")]
        public float radius { get { return Radius; } set { Radius = value; } }
        [Obsolete("Use Height instead.")]
        public float height { get { return Height; } set { Height = value; } }

    }
}

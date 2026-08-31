using System;
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
            get { return ManagedRuntimeContext.Collider2DGetIsTrigger(EntityId); }
            set { ManagedRuntimeContext.Collider2DSetIsTrigger(EntityId, value); }
        }

        public Vector2 Offset
        {
            get { return ManagedRuntimeContext.Collider2DGetOffset(EntityId); }
            set { ManagedRuntimeContext.Collider2DSetOffset(EntityId, value); }
        }

        public PhysicsMaterial2D Material
        {
            get { return ManagedRuntimeContext.CreateAsset<PhysicsMaterial2D>(ManagedRuntimeContext.Collider2DGetMaterial(EntityId)); }
            set { ManagedRuntimeContext.Collider2DSetMaterial(EntityId, value != null ? value.uuid : UUID.Empty); }
        }

        /// <summary>Per-collider values applied after the reusable material asset.</summary>
        public PhysicsMaterialOverride MaterialOverride
        {
            get { return ManagedRuntimeContext.Collider2DGetMaterialOverride(EntityId); }
            set { ManagedRuntimeContext.Collider2DSetMaterialOverride(EntityId, value); }
        }

        [Obsolete("Use IsTrigger instead.")]
        public bool isTrigger { get { return IsTrigger; } set { IsTrigger = value; } }

        [Obsolete("Use Offset instead.")]
        public Vector2 offset { get { return Offset; } set { Offset = value; } }

    }

    /// <summary>A rectangular 2D collision shape.</summary>
    public sealed class BoxCollider2D : Collider2D
    {
        public Vector2 Size
        {
            get { return ManagedRuntimeContext.BoxCollider2DGetSize(EntityId); }
            set { ManagedRuntimeContext.BoxCollider2DSetSize(EntityId, value); }
        }

        [Obsolete("Use Size instead.")]
        public Vector2 size { get { return Size; } set { Size = value; } }

    }

    /// <summary>A circular 2D collision shape.</summary>
    public sealed class CircleCollider2D : Collider2D
    {
        public float Radius
        {
            get { return ManagedRuntimeContext.CircleCollider2DGetRadius(EntityId); }
            set { ManagedRuntimeContext.CircleCollider2DSetRadius(EntityId, value); }
        }

        [Obsolete("Use Radius instead.")]
        public float radius { get { return Radius; } set { Radius = value; } }

    }
}

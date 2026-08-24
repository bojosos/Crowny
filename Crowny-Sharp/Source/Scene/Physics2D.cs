using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Crowny
{
    /// <summary>Identifies the active 2D physics implementation.</summary>
    public enum PhysicsBackend2D
    {
        Box2D = 0
    }

    /// <summary>Filters a 2D scene query by collision layer.</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct PhysicsQueryFilter2D
    {
        public uint LayerMask;

        public PhysicsQueryFilter2D(uint layerMask)
        {
            LayerMask = layerMask;
        }

        public static PhysicsQueryFilter2D Default
        {
            get { return new PhysicsQueryFilter2D(UInt32.MaxValue); }
        }
    }

    /// <summary>Describes one fixture intersected by a 2D ray.</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct RaycastHit2D
    {
        public Vector2 Point;
        public Vector2 Normal;
        public float Fraction;
        public uint EntityId;

        /// <summary>Gets the hit entity, or null if it no longer exists.</summary>
        public Entity HitEntity { get { return Physics2D.ResolveEntity(EntityId); } }
    }

    /// <summary>Provides backend-independent access to the active 2D physics world.</summary>
    public static class Physics2D
    {
        /// <summary>Number of collision layers supported by the Box2D filter.</summary>
        public const int LayerCount = 16;

        private static readonly RaycastHit2D[] EmptyHits = new RaycastHit2D[0];

        /// <summary>Gets the active 2D backend.</summary>
        public static PhysicsBackend2D Backend { get { return Internal_GetBackend(); } }

        /// <summary>Gets whether a 2D scene is being simulated.</summary>
        public static bool IsSimulating { get { return Internal_IsSimulating(); } }

        /// <summary>Gets or sets world gravity.</summary>
        public static Vector2 Gravity
        {
            get { Internal_GetGravity(out Vector2 value); return value; }
            set { Internal_SetGravity(ref value); }
        }

        /// <summary>Gets or sets the Box2D velocity solver iteration count.</summary>
        public static uint VelocityIterations
        {
            get { return Internal_GetVelocityIterations(); }
            set { Internal_SetVelocityIterations(value); }
        }

        /// <summary>Gets or sets the Box2D position solver iteration count.</summary>
        public static uint PositionIterations
        {
            get { return Internal_GetPositionIterations(); }
            set { Internal_SetPositionIterations(value); }
        }

        /// <summary>Gets or sets the material used by 2D colliders without an explicit material.</summary>
        public static PhysicsMaterial2D DefaultMaterial
        {
            get { return Internal_GetDefaultMaterial(); }
            set { Internal_SetDefaultMaterial(value); }
        }

        /// <summary>Casts a ray and returns hits ordered from nearest to farthest.</summary>
        public static RaycastHit2D[] Raycast(Vector2 origin, Vector2 direction, float distance = Single.MaxValue,
                                             uint layerMask = UInt32.MaxValue)
        {
            Internal_Raycast(ref origin, ref direction, distance, layerMask, out RaycastHit2D[] results);
            return results ?? EmptyHits;
        }

        /// <summary>Casts a ray using an explicit query filter.</summary>
        public static RaycastHit2D[] Raycast(Vector2 origin, Vector2 direction, float distance, PhysicsQueryFilter2D filter)
        {
            return Raycast(origin, direction, distance, filter.LayerMask);
        }

        /// <summary>Casts a ray and returns its nearest hit.</summary>
        public static bool Raycast(Vector2 origin, Vector2 direction, out RaycastHit2D hit,
                                   float distance = Single.MaxValue, uint layerMask = UInt32.MaxValue)
        {
            RaycastHit2D[] hits = Raycast(origin, direction, distance, layerMask);
            if (hits.Length != 0)
            {
                hit = hits[0];
                return true;
            }

            hit = default(RaycastHit2D);
            return false;
        }

        /// <summary>Casts a ray into a caller-owned array and returns the number of results written.</summary>
        public static int RaycastNonAlloc(Vector2 origin, Vector2 direction, RaycastHit2D[] results,
                                          float distance = Single.MaxValue, uint layerMask = UInt32.MaxValue)
        {
            if (results == null)
                throw new ArgumentNullException("results");
            if (results.Length == 0)
                return 0;
            return Internal_RaycastNonAlloc(ref origin, ref direction, distance, layerMask, results, results.Length);
        }

        /// <summary>Casts a ray into a caller-owned array using an explicit query filter.</summary>
        public static int RaycastNonAlloc(Vector2 origin, Vector2 direction, RaycastHit2D[] results,
                                          float distance, PhysicsQueryFilter2D filter)
        {
            return RaycastNonAlloc(origin, direction, results, distance, filter.LayerMask);
        }

        /// <summary>Gets the display name of a collision layer.</summary>
        public static string GetLayerName(int layer)
        {
            ValidateLayer(layer);
            return Internal_GetLayerName(layer);
        }

        /// <summary>Sets the display name of a collision layer.</summary>
        public static void SetLayerName(int layer, string name)
        {
            ValidateLayer(layer);
            if (name == null)
                throw new ArgumentNullException("name");
            Internal_SetLayerName(layer, name);
        }

        /// <summary>Gets the layers that collide with the specified layer.</summary>
        public static uint GetLayerMask(int layer)
        {
            ValidateLayer(layer);
            return Internal_GetLayerMask(layer);
        }

        /// <summary>Sets the layers that collide with the specified layer.</summary>
        public static void SetLayerMask(int layer, uint mask)
        {
            ValidateLayer(layer);
            Internal_SetLayerMask(layer, mask);
        }

        private static void ValidateLayer(int layer)
        {
            if (layer < 0 || layer >= LayerCount)
                throw new ArgumentOutOfRangeException("layer", "Physics layers must be in the range 0 through 15.");
        }

        internal static Entity ResolveEntity(uint entityId)
        {
            return Internal_GetEntity(entityId);
        }

        [MethodImpl(MethodImplOptions.InternalCall)] private static extern PhysicsBackend2D Internal_GetBackend();
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern bool Internal_IsSimulating();
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_GetGravity(out Vector2 value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetGravity(ref Vector2 value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern uint Internal_GetVelocityIterations();
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetVelocityIterations(uint value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern uint Internal_GetPositionIterations();
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetPositionIterations(uint value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern PhysicsMaterial2D Internal_GetDefaultMaterial();
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetDefaultMaterial(PhysicsMaterial2D material);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern string Internal_GetLayerName(int layer);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetLayerName(int layer, string name);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern uint Internal_GetLayerMask(int layer);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetLayerMask(int layer, uint mask);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern Entity Internal_GetEntity(uint entityId);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_Raycast(ref Vector2 origin, ref Vector2 direction, float distance,
                                                    uint layerMask, out RaycastHit2D[] results);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int Internal_RaycastNonAlloc(ref Vector2 origin, ref Vector2 direction, float distance,
                                                           uint layerMask, RaycastHit2D[] results, int capacity);
    }
}

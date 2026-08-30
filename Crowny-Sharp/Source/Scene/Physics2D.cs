using System;
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
        /// <summary>Runtime entity handle valid for the current active scene.</summary>
        public uint EntityId;

        /// <summary>Gets the hit entity, or null if it no longer exists.</summary>
        public Entity HitEntity { get { return Physics2D.ResolveEntity(EntityId); } }
    }

    /// <summary>Provides backend-independent access to the active 2D physics world.</summary>
    public static class Physics2D
    {
        public const int LayerCount = 16;
        private const uint InitialQueryCapacity = 16;

        public static PhysicsBackend2D Backend
        {
            get { return (PhysicsBackend2D)ManagedRuntimeContext.Physics2DGetBackend(); }
        }

        public static bool IsSimulating
        {
            get { return ManagedRuntimeContext.Physics2DGetIsSimulating(); }
        }

        public static Vector2 Gravity
        {
            get { return ManagedRuntimeContext.Physics2DGetGravity(); }
            set { ManagedRuntimeContext.Physics2DSetGravity(value); }
        }

        public static uint VelocityIterations
        {
            get { return ManagedRuntimeContext.Physics2DGetVelocityIterations(); }
            set { ManagedRuntimeContext.Physics2DSetVelocityIterations(value); }
        }

        public static uint PositionIterations
        {
            get { return ManagedRuntimeContext.Physics2DGetPositionIterations(); }
            set { ManagedRuntimeContext.Physics2DSetPositionIterations(value); }
        }

        public static PhysicsMaterial2D DefaultMaterial
        {
            get { return ManagedRuntimeContext.CreateAsset<PhysicsMaterial2D>(ManagedRuntimeContext.Physics2DGetDefaultMaterial()); }
            set { ManagedRuntimeContext.Physics2DSetDefaultMaterial(value != null ? value.uuid : UUID.Empty); }
        }

        /// <summary>Casts a ray and returns hits ordered from nearest to farthest.</summary>
        public static RaycastHit2D[] Raycast(Vector2 origin, Vector2 direction, float distance = Single.MaxValue,
                                             uint layerMask = UInt32.MaxValue)
        {
            return ManagedArrayInterop.Query<RaycastHit2D>(InitialQueryCapacity, (destination, capacity) =>
                ManagedRuntimeContext.Physics2DRaycast(origin, direction, distance, layerMask, destination, capacity));
        }

        public static RaycastHit2D[] Raycast(Vector2 origin, Vector2 direction, float distance, PhysicsQueryFilter2D filter)
        {
            return Raycast(origin, direction, distance, filter.LayerMask);
        }

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

        public static int RaycastNonAlloc(Vector2 origin, Vector2 direction, RaycastHit2D[] results,
                                          float distance = Single.MaxValue, uint layerMask = UInt32.MaxValue)
        {
            return ManagedArrayInterop.WriteNonAlloc(results, (destination, capacity) =>
                ManagedRuntimeContext.Physics2DRaycast(origin, direction, distance, layerMask, destination, capacity));
        }

        public static int RaycastNonAlloc(Vector2 origin, Vector2 direction, RaycastHit2D[] results,
                                          float distance, PhysicsQueryFilter2D filter)
        {
            return RaycastNonAlloc(origin, direction, results, distance, filter.LayerMask);
        }

        public static string GetLayerName(int layer)
        {
            ValidateLayer(layer);
            return ManagedRuntimeContext.Physics2DGetLayerName(layer);
        }

        public static void SetLayerName(int layer, string name)
        {
            ValidateLayer(layer);
            if (name == null)
                throw new ArgumentNullException("name");
            ManagedRuntimeContext.Physics2DSetLayerName(layer, name);
        }

        public static uint GetLayerMask(int layer)
        {
            ValidateLayer(layer);
            return ManagedRuntimeContext.Physics2DGetLayerMask(layer);
        }

        public static void SetLayerMask(int layer, uint mask)
        {
            ValidateLayer(layer);
            ManagedRuntimeContext.Physics2DSetLayerMask(layer, mask);
        }

        private static void ValidateLayer(int layer)
        {
            if (layer < 0 || layer >= LayerCount)
                throw new ArgumentOutOfRangeException("layer", "Physics layers must be in the range 0 through 15.");
        }

        internal static Entity ResolveEntity(uint entityId)
        {
            UUID uuid = ManagedRuntimeContext.Physics2DResolveEntity(entityId);
            return uuid == UUID.Empty ? null : new Entity { m_ManagedUuid = uuid };
        }
    }
}

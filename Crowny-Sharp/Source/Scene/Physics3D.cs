using System;
using System.Runtime.InteropServices;

namespace Crowny
{
    /// <summary>Identifies a selectable 3D physics implementation.</summary>
    public enum PhysicsBackend3D
    {
        Box3D = 0,
        Jolt = 1,
        Bullet = 2
    }

    /// <summary>Features that may differ between 3D physics backends.</summary>
    [Flags]
    public enum Physics3DCapability : ulong
    {
        None = 0,
        RigidBodies = 1UL << 0,
        PrimitiveShapes = 1UL << 1,
        ConvexShapes = 1UL << 2,
        TriangleMeshes = 1UL << 3,
        HeightFields = 1UL << 4,
        CompoundShapes = 1UL << 5,
        Sensors = 1UL << 6,
        ContinuousCollision = 1UL << 7,
        Raycasts = 1UL << 8,
        ShapeCasts = 1UL << 9,
        Overlaps = 1UL << 10,
        Constraints = 1UL << 11,
        Motors = 1UL << 12,
        Springs = 1UL << 13,
        CharacterController = 1UL << 14,
        SoftBodies = 1UL << 15,
        Vehicles = 1UL << 16,
        DeterministicSimulation = 1UL << 17
    }

    /// <summary>Controls which bodies collide with or query a 3D body.</summary>
    [StructLayout(LayoutKind.Sequential), SerializeObject]
    public struct PhysicsFilter3D
    {
        public uint Layer;
        public uint Mask;
        public int Group;

        public PhysicsFilter3D(uint layer, uint mask = UInt32.MaxValue, int group = 0)
        {
            Layer = layer;
            Mask = mask;
            Group = group;
        }

        public static PhysicsFilter3D Default
        {
            get { return new PhysicsFilter3D(0, UInt32.MaxValue, 0); }
        }
    }

    /// <summary>Filters a 3D scene query.</summary>
    public struct PhysicsQueryFilter3D
    {
        public uint LayerMask;
        public bool IncludeTriggers;
        public ulong IgnoreBodyHandle;

        public PhysicsQueryFilter3D(uint layerMask, bool includeTriggers = true, ulong ignoreBodyHandle = 0)
        {
            LayerMask = layerMask;
            IncludeTriggers = includeTriggers;
            IgnoreBodyHandle = ignoreBodyHandle;
        }

        public static PhysicsQueryFilter3D Default
        {
            get { return new PhysicsQueryFilter3D(UInt32.MaxValue, true); }
        }
    }

    public enum PhysicsQueryShapeType3D
    {
        Box = 0,
        Sphere = 1,
        Capsule = 2
    }

    /// <summary>Describes a backend-independent primitive query shape.</summary>
    public struct PhysicsQueryShape3D
    {
        public PhysicsQueryShapeType3D Type;
        public Vector3 Size;
        public float Radius;
        public float Height;

        public static PhysicsQueryShape3D Box(Vector3 size)
        {
            PhysicsQueryShape3D shape = new PhysicsQueryShape3D();
            shape.Type = PhysicsQueryShapeType3D.Box;
            shape.Size = size;
            return shape;
        }

        public static PhysicsQueryShape3D Sphere(float radius)
        {
            PhysicsQueryShape3D shape = new PhysicsQueryShape3D();
            shape.Type = PhysicsQueryShapeType3D.Sphere;
            shape.Radius = radius;
            return shape;
        }

        public static PhysicsQueryShape3D Capsule(float radius, float height)
        {
            PhysicsQueryShape3D shape = new PhysicsQueryShape3D();
            shape.Type = PhysicsQueryShapeType3D.Capsule;
            shape.Radius = radius;
            shape.Height = height;
            return shape;
        }
    }

    /// <summary>Describes one body intersected by a 3D scene query.</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct RaycastHit3D
    {
        public Vector3 Point;
        public Vector3 Normal;
        public float Distance;
        public float Fraction;
        public ulong BodyHandle;
        public ulong ShapeHandle;
        /// <summary>Runtime entity handle valid for the current active scene.</summary>
        public ulong EntityId;

        public Entity HitEntity { get { return Physics3D.ResolveEntity(EntityId); } }
    }

    /// <summary>Provides backend-independent access to the active 3D physics world.</summary>
    public static class Physics3D
    {
        private const uint InitialQueryCapacity = 16;

        public static PhysicsBackend3D Backend
        {
            get { return (PhysicsBackend3D)ManagedRuntimeContext.Physics3DGetBackend(); }
        }

        public static string BackendName { get { return ManagedRuntimeContext.Physics3DGetBackendName(); } }
        public static bool IsSimulating { get { return ManagedRuntimeContext.Physics3DGetIsSimulating(); } }

        public static Physics3DCapability Capabilities
        {
            get { return (Physics3DCapability)ManagedRuntimeContext.Physics3DGetCapabilities(); }
        }

        public static Vector3 Gravity
        {
            get { return ManagedRuntimeContext.Physics3DGetGravity(); }
            set { ManagedRuntimeContext.Physics3DSetGravity(value); }
        }

        public static uint Substeps
        {
            get { return ManagedRuntimeContext.Physics3DGetSubsteps(); }
            set { ManagedRuntimeContext.Physics3DSetSubsteps(value); }
        }

        public static PhysicsMaterial3D DefaultMaterial
        {
            get { return ManagedRuntimeContext.CreateAsset<PhysicsMaterial3D>(ManagedRuntimeContext.Physics3DGetDefaultMaterial()); }
            set { ManagedRuntimeContext.Physics3DSetDefaultMaterial(value != null ? value.uuid : UUID.Empty); }
        }

        public static bool TrySetBackend(PhysicsBackend3D value)
        {
            return ManagedRuntimeContext.Physics3DTrySetBackend((int)value);
        }

        public static bool IsBackendAvailable(PhysicsBackend3D value)
        {
            return ManagedRuntimeContext.Physics3DIsBackendAvailable((int)value);
        }

        public static bool Supports(Physics3DCapability capability)
        {
            return (Capabilities & capability) == capability;
        }

        public static RaycastHit3D[] Raycast(Vector3 origin, Vector3 direction, float distance = Single.MaxValue,
                                             uint layerMask = UInt32.MaxValue, bool includeTriggers = true)
        {
            return Raycast(origin, direction, distance, new PhysicsQueryFilter3D(layerMask, includeTriggers));
        }

        public static RaycastHit3D[] Raycast(Vector3 origin, Vector3 direction, float distance, PhysicsQueryFilter3D filter)
        {
            return ManagedArrayInterop.Query<RaycastHit3D>(InitialQueryCapacity, (destination, capacity) =>
                QueryRaycast(origin, direction, distance, filter, destination, capacity));
        }

        public static bool Raycast(Vector3 origin, Vector3 direction, out RaycastHit3D hit,
                                   float distance = Single.MaxValue, uint layerMask = UInt32.MaxValue,
                                   bool includeTriggers = true)
        {
            return FirstHit(Raycast(origin, direction, distance, layerMask, includeTriggers), out hit);
        }

        public static int RaycastNonAlloc(Vector3 origin, Vector3 direction, RaycastHit3D[] results,
                                          float distance = Single.MaxValue, uint layerMask = UInt32.MaxValue,
                                          bool includeTriggers = true)
        {
            return RaycastNonAlloc(origin, direction, results, distance,
                                   new PhysicsQueryFilter3D(layerMask, includeTriggers));
        }

        public static int RaycastNonAlloc(Vector3 origin, Vector3 direction, RaycastHit3D[] results,
                                          float distance, PhysicsQueryFilter3D filter)
        {
            return ManagedArrayInterop.WriteNonAlloc(results, (destination, capacity) =>
                QueryRaycast(origin, direction, distance, filter, destination, capacity));
        }

        public static RaycastHit3D[] Sweep(PhysicsQueryShape3D shape, Vector3 position, Quaternion rotation,
                                           Vector3 direction, float distance, PhysicsQueryFilter3D filter)
        {
            return ManagedArrayInterop.Query<RaycastHit3D>(InitialQueryCapacity, (destination, capacity) =>
                QuerySweep(shape, position, rotation, direction, distance, filter, destination, capacity));
        }

        public static bool Sweep(PhysicsQueryShape3D shape, Vector3 position, Quaternion rotation, Vector3 direction,
                                 out RaycastHit3D hit, float distance = Single.MaxValue,
                                 uint layerMask = UInt32.MaxValue, bool includeTriggers = true)
        {
            return FirstHit(Sweep(shape, position, rotation, direction, distance,
                                  new PhysicsQueryFilter3D(layerMask, includeTriggers)), out hit);
        }

        public static int SweepNonAlloc(PhysicsQueryShape3D shape, Vector3 position, Quaternion rotation,
                                        Vector3 direction, RaycastHit3D[] results, float distance,
                                        PhysicsQueryFilter3D filter)
        {
            return ManagedArrayInterop.WriteNonAlloc(results, (destination, capacity) =>
                QuerySweep(shape, position, rotation, direction, distance, filter, destination, capacity));
        }

        public static int SweepNonAlloc(PhysicsQueryShape3D shape, Vector3 position, Quaternion rotation,
                                        Vector3 direction, RaycastHit3D[] results, float distance = Single.MaxValue,
                                        uint layerMask = UInt32.MaxValue, bool includeTriggers = true)
        {
            return SweepNonAlloc(shape, position, rotation, direction, results, distance,
                                 new PhysicsQueryFilter3D(layerMask, includeTriggers));
        }

        public static RaycastHit3D[] Overlap(PhysicsQueryShape3D shape, Vector3 position, Quaternion rotation,
                                             PhysicsQueryFilter3D filter)
        {
            return ManagedArrayInterop.Query<RaycastHit3D>(InitialQueryCapacity, (destination, capacity) =>
                QueryOverlap(shape, position, rotation, filter, destination, capacity));
        }

        public static RaycastHit3D[] Overlap(PhysicsQueryShape3D shape, Vector3 position, Quaternion rotation,
                                             uint layerMask = UInt32.MaxValue, bool includeTriggers = true)
        {
            return Overlap(shape, position, rotation, new PhysicsQueryFilter3D(layerMask, includeTriggers));
        }

        public static int OverlapNonAlloc(PhysicsQueryShape3D shape, Vector3 position, Quaternion rotation,
                                          RaycastHit3D[] results, PhysicsQueryFilter3D filter)
        {
            return ManagedArrayInterop.WriteNonAlloc(results, (destination, capacity) =>
                QueryOverlap(shape, position, rotation, filter, destination, capacity));
        }

        public static int OverlapNonAlloc(PhysicsQueryShape3D shape, Vector3 position, Quaternion rotation,
                                          RaycastHit3D[] results, uint layerMask = UInt32.MaxValue,
                                          bool includeTriggers = true)
        {
            return OverlapNonAlloc(shape, position, rotation, results,
                                   new PhysicsQueryFilter3D(layerMask, includeTriggers));
        }

        private static uint QueryRaycast(Vector3 origin, Vector3 direction, float distance, PhysicsQueryFilter3D filter,
                                         IntPtr destination, uint capacity)
        {
            return ManagedRuntimeContext.Physics3DRaycast(origin, direction, distance, filter.LayerMask,
                filter.IncludeTriggers, filter.IgnoreBodyHandle, destination, capacity);
        }

        private static uint QuerySweep(PhysicsQueryShape3D shape, Vector3 position, Quaternion rotation,
                                       Vector3 direction, float distance, PhysicsQueryFilter3D filter,
                                       IntPtr destination, uint capacity)
        {
            return ManagedRuntimeContext.Physics3DSweep((int)shape.Type, shape.Size, shape.Radius, shape.Height,
                position, rotation, direction, distance, filter.LayerMask, filter.IncludeTriggers,
                filter.IgnoreBodyHandle, destination, capacity);
        }

        private static uint QueryOverlap(PhysicsQueryShape3D shape, Vector3 position, Quaternion rotation,
                                         PhysicsQueryFilter3D filter, IntPtr destination, uint capacity)
        {
            return ManagedRuntimeContext.Physics3DOverlap((int)shape.Type, shape.Size, shape.Radius, shape.Height,
                position, rotation, filter.LayerMask, filter.IncludeTriggers, filter.IgnoreBodyHandle,
                destination, capacity);
        }

        private static bool FirstHit(RaycastHit3D[] hits, out RaycastHit3D hit)
        {
            if (hits.Length != 0)
            {
                hit = hits[0];
                return true;
            }
            hit = default(RaycastHit3D);
            return false;
        }

        internal static Entity ResolveEntity(ulong entityId)
        {
            UUID uuid = ManagedRuntimeContext.Physics3DResolveEntity(entityId);
            return uuid == UUID.Empty ? null : new Entity { m_ManagedUuid = uuid };
        }
    }
}

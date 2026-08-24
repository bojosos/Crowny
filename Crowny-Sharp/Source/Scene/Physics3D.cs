using System;
using System.Runtime.CompilerServices;
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
        /// <summary>Runtime body handle excluded from the query. Zero excludes nothing.</summary>
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

    /// <summary>Primitive shape used by a sweep or overlap query.</summary>
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

        /// <summary>Creates a box query shape. Size contains the full width, height, and depth.</summary>
        public static PhysicsQueryShape3D Box(Vector3 size)
        {
            PhysicsQueryShape3D shape = new PhysicsQueryShape3D();
            shape.Type = PhysicsQueryShapeType3D.Box;
            shape.Size = size;
            return shape;
        }

        /// <summary>Creates a sphere query shape.</summary>
        public static PhysicsQueryShape3D Sphere(float radius)
        {
            PhysicsQueryShape3D shape = new PhysicsQueryShape3D();
            shape.Type = PhysicsQueryShapeType3D.Sphere;
            shape.Radius = radius;
            return shape;
        }

        /// <summary>Creates a capsule query shape.</summary>
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
        /// <summary>Runtime-only backend-independent body handle.</summary>
        public ulong BodyHandle;
        /// <summary>Runtime-only backend-independent shape handle.</summary>
        public ulong ShapeHandle;
        public ulong EntityId;

        /// <summary>Gets the hit entity, or null if it no longer exists.</summary>
        public Entity HitEntity { get { return Physics3D.ResolveEntity(EntityId); } }
    }

    /// <summary>Provides backend-independent access to the active 3D physics world.</summary>
    public static class Physics3D
    {
        private static readonly RaycastHit3D[] EmptyHits = new RaycastHit3D[0];

        /// <summary>Gets the active backend.</summary>
        public static PhysicsBackend3D Backend { get { return Internal_GetBackend(); } }

        /// <summary>Gets the active backend's display name.</summary>
        public static string BackendName { get { return Internal_GetBackendName(); } }

        /// <summary>Gets whether a 3D scene is being simulated.</summary>
        public static bool IsSimulating { get { return Internal_IsSimulating(); } }

        /// <summary>Gets the features implemented by the active backend.</summary>
        public static Physics3DCapability Capabilities
        {
            get { return (Physics3DCapability)Internal_GetCapabilities(); }
        }

        /// <summary>Gets or sets world gravity.</summary>
        public static Vector3 Gravity
        {
            get { Internal_GetGravity(out Vector3 value); return value; }
            set { Internal_SetGravity(ref value); }
        }

        /// <summary>Gets or sets the number of solver substeps per fixed update.</summary>
        public static uint Substeps
        {
            get { return Internal_GetSubsteps(); }
            set { Internal_SetSubsteps(value); }
        }

        /// <summary>Gets or sets the material assigned to newly created 3D colliders.</summary>
        public static PhysicsMaterial3D DefaultMaterial
        {
            get { return Internal_GetDefaultMaterial(); }
            set { Internal_SetDefaultMaterial(value); }
        }

        /// <summary>Changes backend while simulation is stopped.</summary>
        public static bool TrySetBackend(PhysicsBackend3D value)
        {
            return Internal_TrySetBackend(value);
        }

        /// <summary>Checks whether this build includes a backend.</summary>
        public static bool IsBackendAvailable(PhysicsBackend3D value)
        {
            return Internal_IsBackendAvailable(value);
        }

        /// <summary>Checks whether the active backend implements a feature.</summary>
        public static bool Supports(Physics3DCapability capability)
        {
            return (Capabilities & capability) == capability;
        }

        /// <summary>Casts a ray and returns hits ordered from nearest to farthest.</summary>
        public static RaycastHit3D[] Raycast(Vector3 origin, Vector3 direction, float distance = Single.MaxValue,
                                             uint layerMask = UInt32.MaxValue, bool includeTriggers = true)
        {
            Internal_Raycast(ref origin, ref direction, distance, layerMask, includeTriggers, 0,
                             out RaycastHit3D[] results);
            return results ?? EmptyHits;
        }

        /// <summary>Casts a ray using an explicit query filter.</summary>
        public static RaycastHit3D[] Raycast(Vector3 origin, Vector3 direction, float distance, PhysicsQueryFilter3D filter)
        {
            Internal_Raycast(ref origin, ref direction, distance, filter.LayerMask, filter.IncludeTriggers,
                             filter.IgnoreBodyHandle, out RaycastHit3D[] results);
            return results ?? EmptyHits;
        }

        /// <summary>Casts a ray and returns its nearest hit.</summary>
        public static bool Raycast(Vector3 origin, Vector3 direction, out RaycastHit3D hit,
                                   float distance = Single.MaxValue, uint layerMask = UInt32.MaxValue,
                                   bool includeTriggers = true)
        {
            RaycastHit3D[] hits = Raycast(origin, direction, distance, layerMask, includeTriggers);
            return FirstHit(hits, out hit);
        }

        /// <summary>Casts a ray into a caller-owned array and returns the number of results written.</summary>
        public static int RaycastNonAlloc(Vector3 origin, Vector3 direction, RaycastHit3D[] results,
                                          float distance = Single.MaxValue, uint layerMask = UInt32.MaxValue,
                                          bool includeTriggers = true)
        {
            ValidateResults(results);
            if (results.Length == 0)
                return 0;
            return Internal_RaycastNonAlloc(ref origin, ref direction, distance, layerMask, includeTriggers, 0,
                                            results, results.Length);
        }

        /// <summary>Casts a ray into a caller-owned array using an explicit query filter.</summary>
        public static int RaycastNonAlloc(Vector3 origin, Vector3 direction, RaycastHit3D[] results,
                                          float distance, PhysicsQueryFilter3D filter)
        {
            ValidateResults(results);
            if (results.Length == 0)
                return 0;
            return Internal_RaycastNonAlloc(ref origin, ref direction, distance, filter.LayerMask,
                                            filter.IncludeTriggers, filter.IgnoreBodyHandle, results, results.Length);
        }

        /// <summary>Sweeps a primitive shape through the scene.</summary>
        public static RaycastHit3D[] Sweep(PhysicsQueryShape3D shape, Vector3 position, Quaternion rotation,
                                           Vector3 direction, float distance, PhysicsQueryFilter3D filter)
        {
            Internal_Sweep((int)shape.Type, ref shape.Size, shape.Radius, shape.Height, ref position, ref rotation,
                           ref direction, distance, filter.LayerMask, filter.IncludeTriggers, filter.IgnoreBodyHandle,
                           out RaycastHit3D[] results);
            return results ?? EmptyHits;
        }

        /// <summary>Sweeps a primitive shape and returns its nearest hit.</summary>
        public static bool Sweep(PhysicsQueryShape3D shape, Vector3 position, Quaternion rotation, Vector3 direction,
                                 out RaycastHit3D hit, float distance = Single.MaxValue,
                                 uint layerMask = UInt32.MaxValue, bool includeTriggers = true)
        {
            RaycastHit3D[] hits = Sweep(shape, position, rotation, direction, distance,
                new PhysicsQueryFilter3D(layerMask, includeTriggers));
            return FirstHit(hits, out hit);
        }

        /// <summary>Sweeps a primitive shape into a caller-owned array.</summary>
        public static int SweepNonAlloc(PhysicsQueryShape3D shape, Vector3 position, Quaternion rotation,
                                        Vector3 direction, RaycastHit3D[] results, float distance,
                                        PhysicsQueryFilter3D filter)
        {
            ValidateResults(results);
            if (results.Length == 0)
                return 0;
            return Internal_SweepNonAlloc((int)shape.Type, ref shape.Size, shape.Radius, shape.Height,
                                          ref position, ref rotation, ref direction, distance, filter.LayerMask,
                                          filter.IncludeTriggers, filter.IgnoreBodyHandle, results, results.Length);
        }

        /// <summary>Sweeps a primitive shape into a caller-owned array.</summary>
        public static int SweepNonAlloc(PhysicsQueryShape3D shape, Vector3 position, Quaternion rotation,
                                        Vector3 direction, RaycastHit3D[] results, float distance = Single.MaxValue,
                                        uint layerMask = UInt32.MaxValue, bool includeTriggers = true)
        {
            return SweepNonAlloc(shape, position, rotation, direction, results, distance,
                                 new PhysicsQueryFilter3D(layerMask, includeTriggers));
        }

        /// <summary>Finds bodies overlapping a primitive shape.</summary>
        public static RaycastHit3D[] Overlap(PhysicsQueryShape3D shape, Vector3 position, Quaternion rotation,
                                             PhysicsQueryFilter3D filter)
        {
            Internal_Overlap((int)shape.Type, ref shape.Size, shape.Radius, shape.Height, ref position, ref rotation,
                             filter.LayerMask, filter.IncludeTriggers, filter.IgnoreBodyHandle,
                             out RaycastHit3D[] results);
            return results ?? EmptyHits;
        }

        /// <summary>Finds bodies overlapping a primitive shape.</summary>
        public static RaycastHit3D[] Overlap(PhysicsQueryShape3D shape, Vector3 position, Quaternion rotation,
                                             uint layerMask = UInt32.MaxValue, bool includeTriggers = true)
        {
            return Overlap(shape, position, rotation, new PhysicsQueryFilter3D(layerMask, includeTriggers));
        }

        /// <summary>Writes overlapping bodies into a caller-owned array.</summary>
        public static int OverlapNonAlloc(PhysicsQueryShape3D shape, Vector3 position, Quaternion rotation,
                                          RaycastHit3D[] results, PhysicsQueryFilter3D filter)
        {
            ValidateResults(results);
            if (results.Length == 0)
                return 0;
            return Internal_OverlapNonAlloc((int)shape.Type, ref shape.Size, shape.Radius, shape.Height,
                                            ref position, ref rotation, filter.LayerMask, filter.IncludeTriggers,
                                            filter.IgnoreBodyHandle,
                                            results, results.Length);
        }

        /// <summary>Writes overlapping bodies into a caller-owned array.</summary>
        public static int OverlapNonAlloc(PhysicsQueryShape3D shape, Vector3 position, Quaternion rotation,
                                          RaycastHit3D[] results, uint layerMask = UInt32.MaxValue,
                                          bool includeTriggers = true)
        {
            return OverlapNonAlloc(shape, position, rotation, results,
                                   new PhysicsQueryFilter3D(layerMask, includeTriggers));
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

        private static void ValidateResults(RaycastHit3D[] results)
        {
            if (results == null)
                throw new ArgumentNullException("results");
        }

        internal static Entity ResolveEntity(ulong entityId)
        {
            return Internal_GetEntity(entityId);
        }

        [MethodImpl(MethodImplOptions.InternalCall)] private static extern PhysicsBackend3D Internal_GetBackend();
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern string Internal_GetBackendName();
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern bool Internal_IsSimulating();
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern ulong Internal_GetCapabilities();
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_GetGravity(out Vector3 value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetGravity(ref Vector3 value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern uint Internal_GetSubsteps();
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetSubsteps(uint value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern PhysicsMaterial3D Internal_GetDefaultMaterial();
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetDefaultMaterial(PhysicsMaterial3D value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern bool Internal_TrySetBackend(PhysicsBackend3D value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern bool Internal_IsBackendAvailable(PhysicsBackend3D value);
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern Entity Internal_GetEntity(ulong entityId);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_Raycast(ref Vector3 origin, ref Vector3 direction, float distance,
                                                    uint layerMask, bool includeTriggers, ulong ignoreBodyHandle,
                                                    out RaycastHit3D[] results);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int Internal_RaycastNonAlloc(ref Vector3 origin, ref Vector3 direction, float distance,
                                                           uint layerMask, bool includeTriggers, ulong ignoreBodyHandle,
                                                           RaycastHit3D[] results, int capacity);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_Sweep(int shapeType, ref Vector3 size, float radius, float height,
                                                  ref Vector3 position, ref Quaternion rotation, ref Vector3 direction,
                                                  float distance, uint layerMask, bool includeTriggers,
                                                  ulong ignoreBodyHandle,
                                                  out RaycastHit3D[] results);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int Internal_SweepNonAlloc(int shapeType, ref Vector3 size, float radius, float height,
                                                         ref Vector3 position, ref Quaternion rotation,
                                                         ref Vector3 direction, float distance, uint layerMask,
                                                         bool includeTriggers, ulong ignoreBodyHandle,
                                                         RaycastHit3D[] results, int capacity);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_Overlap(int shapeType, ref Vector3 size, float radius, float height,
                                                    ref Vector3 position, ref Quaternion rotation, uint layerMask,
                                                    bool includeTriggers, ulong ignoreBodyHandle,
                                                    out RaycastHit3D[] results);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int Internal_OverlapNonAlloc(int shapeType, ref Vector3 size, float radius, float height,
                                                           ref Vector3 position, ref Quaternion rotation,
                                                           uint layerMask, bool includeTriggers, ulong ignoreBodyHandle,
                                                           RaycastHit3D[] results, int capacity);
    }
}

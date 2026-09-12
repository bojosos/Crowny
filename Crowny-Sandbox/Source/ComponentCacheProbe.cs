using System;
using Crowny;

namespace Sandbox
{
    /// <summary>Exercises native component caching without caching managed script instances.</summary>
    public class ComponentCacheProbe : EntityBehaviour
    {
        /// <summary>Selects the integration test operation.</summary>
        public int Stage;
        /// <summary>Whether transform lookups share the same native wrapper.</summary>
        public bool TransformReused;
        /// <summary>Whether component.entity reuses its entity wrapper.</summary>
        public bool EntityReused;
        /// <summary>Whether optional component lookups reuse a live wrapper.</summary>
        public bool CameraReused;
        /// <summary>Whether a removed native component disappears from lookup.</summary>
        public bool RemovedCameraMissing;
        /// <summary>Whether a replacement component gets a new wrapper.</summary>
        public bool CameraReplaced;
        /// <summary>Whether a cached transform rejects a destroyed entity.</summary>
        public bool DestroyedEntityRejected;
        /// <summary>Whether a scene change renews a cached transform.</summary>
        public bool SceneCacheRenewed;
        /// <summary>Sum of repeated native position reads.</summary>
        public float PositionSum;
        /// <summary>Position read after a scene change with a reused entity UUID.</summary>
        public float ScenePosition;
        /// <summary>Bytes allocated by warm transform.position reads where supported.</summary>
        public long WarmReadAllocatedBytes;

        private Transform previousTransform;
        private Entity target;
        private Transform targetTransform;
        private Camera previousCamera;

        private void Update()
        {
            switch (Stage)
            {
                case 0:
                    previousTransform = transform;
                    Entity owner = entity;
                    TransformReused = ReferenceEquals(previousTransform, GetComponent<Transform>()) &&
                        ReferenceEquals(previousTransform, owner.transform) &&
                        ReferenceEquals(previousTransform, previousTransform.transform);
                    EntityReused = ReferenceEquals(owner, entity) && ReferenceEquals(owner, previousTransform.entity);
                    // Warm the native transport before measuring the repeated property path.
                    PositionSum = transform.position.x;
#if NET10_0
                    long before = GC.GetAllocatedBytesForCurrentThread();
#endif
                    for (int i = 0; i < 512; ++i)
                        PositionSum += transform.position.x;
#if NET10_0
                    WarmReadAllocatedBytes = GC.GetAllocatedBytesForCurrentThread() - before;
#endif
                    break;
                case 1:
                    target = Entity.FindByName("Component cache target");
                    targetTransform = target.transform;
                    previousCamera = target.GetComponent<Camera>();
                    CameraReused = previousCamera != null && ReferenceEquals(previousCamera, target.GetComponent<Camera>());
                    break;
                case 2:
                    RemovedCameraMissing = target.GetComponent<Camera>() == null;
                    break;
                case 3:
                    Camera current = target.GetComponent<Camera>();
                    CameraReplaced = current != null && !ReferenceEquals(previousCamera, current);
                    previousCamera = current;
                    break;
                case 4:
                    target.RemoveComponent<Camera>();
                    Camera replacement = target.AddComponent<Camera>();
                    CameraReplaced = replacement != null && !ReferenceEquals(previousCamera, replacement) &&
                        ReferenceEquals(replacement, target.GetComponent<Camera>());
                    break;
                case 5:
                    try
                    {
                        PositionSum = targetTransform.position.x;
                    }
                    catch (InvalidOperationException)
                    {
                        DestroyedEntityRejected = true;
                    }
                    break;
                case 6:
                    SceneCacheRenewed = !ReferenceEquals(previousTransform, transform);
                    ScenePosition = transform.position.x;
                    break;
            }
        }
    }
}

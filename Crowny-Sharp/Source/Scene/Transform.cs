namespace Crowny
{
    public class Transform : Component
    {
        public enum DirtyFlag
        {
            LocalTransformDirty,
            GlobalTransformDirty,
        }

        /// <summary>The position in world space.</summary>
        public Vector3 position
        {
            get { return ManagedRuntimeContext.TransformGetPosition(EntityId); }
            set { ManagedRuntimeContext.TransformSetPosition(EntityId, value); }
        }

        /// <summary>The position in local space.</summary>
        public Vector3 localPosition
        {
            get { return ManagedRuntimeContext.TransformGetLocalPosition(EntityId); }
            set { ManagedRuntimeContext.TransformSetLocalPosition(EntityId, value); }
        }

        /// <summary>The scale in world space.</summary>
        public Vector3 scale
        {
            get { return ManagedRuntimeContext.TransformGetScale(EntityId); }
            set { ManagedRuntimeContext.TransformSetScale(EntityId, value); }
        }

        /// <summary>The scale in local space.</summary>
        public Vector3 localScale
        {
            get { return ManagedRuntimeContext.TransformGetLocalScale(EntityId); }
            set { ManagedRuntimeContext.TransformSetLocalScale(EntityId, value); }
        }

        /// <summary>The rotation in world space.</summary>
        public Quaternion rotation
        {
            get { return ManagedRuntimeContext.TransformGetRotation(EntityId); }
            set { ManagedRuntimeContext.TransformSetRotation(EntityId, value); }
        }

        /// <summary>The rotation in local space.</summary>
        public Quaternion localRotation
        {
            get { return ManagedRuntimeContext.TransformGetLocalRotation(EntityId); }
            set { ManagedRuntimeContext.TransformSetLocalRotation(EntityId, value); }
        }

        /// <summary>The Euler-angle rotation in world space.</summary>
        public Vector3 eulerAngles
        {
            get { return ManagedRuntimeContext.TransformGetEulerAngles(EntityId); }
            set { ManagedRuntimeContext.TransformSetEulerAngles(EntityId, value); }
        }

        /// <summary>The Euler-angle rotation in local space.</summary>
        public Vector3 localEulerAngles
        {
            get { return ManagedRuntimeContext.TransformGetLocalEulerAngles(EntityId); }
            set { ManagedRuntimeContext.TransformSetLocalEulerAngles(EntityId, value); }
        }

        /// <summary>A matrix that transforms world-space points to local space.</summary>
        public Matrix4 worldToLocalMatrix => ManagedRuntimeContext.TransformGetWorldToLocalMatrix(EntityId);

        /// <summary>A matrix that transforms local-space points to world space.</summary>
        public Matrix4 localToWorldMatrix => ManagedRuntimeContext.TransformGetLocalToWorldMatrix(EntityId);

        /// <summary>Returns whether the selected transform state changed after the last frame.</summary>
        public bool IsDirty(DirtyFlag dirtyFlag) => ManagedRuntimeContext.TransformIsDirty(EntityId, (int)dirtyFlag);
    }
}

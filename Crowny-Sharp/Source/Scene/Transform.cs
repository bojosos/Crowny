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
            get { return ManagedRuntimeContext.TransformGetPosition(entity.uuid); }
            set { ManagedRuntimeContext.TransformSetPosition(entity.uuid, value); }
        }

        /// <summary>The position in local space.</summary>
        public Vector3 localPosition
        {
            get { return ManagedRuntimeContext.TransformGetLocalPosition(entity.uuid); }
            set { ManagedRuntimeContext.TransformSetLocalPosition(entity.uuid, value); }
        }

        /// <summary>The scale in world space.</summary>
        public Vector3 scale
        {
            get { return ManagedRuntimeContext.TransformGetScale(entity.uuid); }
            set { ManagedRuntimeContext.TransformSetScale(entity.uuid, value); }
        }

        /// <summary>The scale in local space.</summary>
        public Vector3 localScale
        {
            get { return ManagedRuntimeContext.TransformGetLocalScale(entity.uuid); }
            set { ManagedRuntimeContext.TransformSetLocalScale(entity.uuid, value); }
        }

        /// <summary>The rotation in world space.</summary>
        public Quaternion rotation
        {
            get { return ManagedRuntimeContext.TransformGetRotation(entity.uuid); }
            set { ManagedRuntimeContext.TransformSetRotation(entity.uuid, value); }
        }

        /// <summary>The rotation in local space.</summary>
        public Quaternion localRotation
        {
            get { return ManagedRuntimeContext.TransformGetLocalRotation(entity.uuid); }
            set { ManagedRuntimeContext.TransformSetLocalRotation(entity.uuid, value); }
        }

        /// <summary>The Euler-angle rotation in world space.</summary>
        public Vector3 eulerAngles
        {
            get { return ManagedRuntimeContext.TransformGetEulerAngles(entity.uuid); }
            set { ManagedRuntimeContext.TransformSetEulerAngles(entity.uuid, value); }
        }

        /// <summary>The Euler-angle rotation in local space.</summary>
        public Vector3 localEulerAngles
        {
            get { return ManagedRuntimeContext.TransformGetLocalEulerAngles(entity.uuid); }
            set { ManagedRuntimeContext.TransformSetLocalEulerAngles(entity.uuid, value); }
        }

        /// <summary>A matrix that transforms world-space points to local space.</summary>
        public Matrix4 worldToLocalMatrix => ManagedRuntimeContext.TransformGetWorldToLocalMatrix(entity.uuid);

        /// <summary>A matrix that transforms local-space points to world space.</summary>
        public Matrix4 localToWorldMatrix => ManagedRuntimeContext.TransformGetLocalToWorldMatrix(entity.uuid);

        /// <summary>Returns whether the selected transform state changed after the last frame.</summary>
        public bool IsDirty(DirtyFlag dirtyFlag) => ManagedRuntimeContext.TransformIsDirty(entity.uuid, (int)dirtyFlag);
    }
}

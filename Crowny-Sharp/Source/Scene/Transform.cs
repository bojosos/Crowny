using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    public class Transform : Component
    {
        // The list of dirty flags that a Transform has.
        public enum DirtyFlag
        {
            /// <summary>
            /// Will be set if the local transform has changed.
            /// </summary>
            LocalTransformDirty,
            /// <summary>
            /// Will be set if the global transform has changed.
            /// </summary>
            GlobalTransformDirty,
        }

        /// <summary>
        /// The position of the transform in world space.
        /// </summary>
        /// <value>A Vector3 world position.</value>
        public Vector3 position
        {
            get
            {
#if CROWNY_MONO
                Internal_GetPosition(m_InternalPtr, out Vector3 tmp);
                return tmp;
#else
                return ManagedRuntimeContext.TransformGetPosition(entity.uuid);
#endif
            }
            set
            {
#if CROWNY_MONO
                Internal_SetPosition(m_InternalPtr, ref value);
#else
                ManagedRuntimeContext.TransformSetPosition(entity.uuid, value);
#endif
            }
        }

        /// <summary>
        /// The position of the transform in local space.
        /// </summary>
        /// <value>A Vector3 local position.</value>
        public Vector3 localPosition 
        { 
            get
            {
#if CROWNY_MONO
                Internal_GetLocalPosition(m_InternalPtr, out Vector3 temp);
                return temp;
#else
                return ManagedRuntimeContext.TransformGetLocalPosition(entity.uuid);
#endif
            }
            set
            {
#if CROWNY_MONO
                Internal_SetLocalPosition(m_InternalPtr, ref value);
#else
                ManagedRuntimeContext.TransformSetLocalPosition(entity.uuid, value);
#endif
            }
        }

        /// <summary>
        /// The scale of the transform in local space.
        /// </summary>
        /// <value>A vector3 local scale.</value>
        public Vector3 scale
        {
            get
            {
#if CROWNY_MONO
                Internal_GetScale(m_InternalPtr, out Vector3 tmp);
                return tmp;
#else
                return ManagedRuntimeContext.TransformGetScale(entity.uuid);
#endif
            }
            set
            {
#if CROWNY_MONO
                Internal_SetScale(m_InternalPtr, ref value);
#else
                ManagedRuntimeContext.TransformSetScale(entity.uuid, value);
#endif
            }
        }

        /// <summary>
        /// Local scale of the transform.
        /// </summary>
        /// <value>A Vector3 local scale.</value>
        public Vector3 localScale 
        {
            get
            {
#if CROWNY_MONO
                Vector3 temp;
                Internal_GetLocalScale(m_InternalPtr, out temp);
                return temp;
#else
                return ManagedRuntimeContext.TransformGetLocalScale(entity.uuid);
#endif
            }
            set
            {
#if CROWNY_MONO
                Internal_SetLocalScale(m_InternalPtr, ref value);
#else
                ManagedRuntimeContext.TransformSetLocalScale(entity.uuid, value);
#endif
            }
        }

        /// <summary>
        /// The quaternion rotation of the transform in world space.
        /// </summary>
        /// <value>A Quaternion global rotation.</value>
        public Quaternion rotation
        {
            get
            {
#if CROWNY_MONO
                Internal_GetRotation(m_InternalPtr, out Quaternion tmp);
                return tmp;
#else
                return ManagedRuntimeContext.TransformGetRotation(entity.uuid);
#endif
            }
            set
            {
#if CROWNY_MONO
                Internal_SetRotation(m_InternalPtr, ref value);
#else
                ManagedRuntimeContext.TransformSetRotation(entity.uuid, value);
#endif
            }
        }

        /// <summary>
        /// The quaternion rotation of the transform in local space.
        /// </summary>
        /// <value>A Quaternion global rotation.</value>
        public Quaternion localRotation
        {
            get
            {
#if CROWNY_MONO
                Internal_GetLocalRotation(m_InternalPtr, out Quaternion tmp);
                return tmp;
#else
                return ManagedRuntimeContext.TransformGetLocalRotation(entity.uuid);
#endif
            }
            set
            {
#if CROWNY_MONO
                Internal_SetLocalRotation(m_InternalPtr, ref value);
#else
                ManagedRuntimeContext.TransformSetLocalRotation(entity.uuid, value);
#endif
            }
        }

        /// <summary>
        /// Euler angle rotation of the transform.
        /// </summary>
        /// <value>A Vector3 world euler rotation.</value>
        public Vector3 eulerAngles
        {
            get
            {
#if CROWNY_MONO
                Internal_GetEulerAngles(m_InternalPtr, out Vector3 temp);
                return temp;
#else
                return ManagedRuntimeContext.TransformGetEulerAngles(entity.uuid);
#endif
            }
            set
            {
#if CROWNY_MONO
                Internal_SetEulerAngles(m_InternalPtr, ref value);
#else
                ManagedRuntimeContext.TransformSetEulerAngles(entity.uuid, value);
#endif
            }
        }

        /// <summary>
        /// Euler angle local rotation of the transform.
        /// </summary>
        /// <value>A Vector3 local euler rotation.</value>
        public Vector3 localEulerAngles
        {
            get
            {
#if CROWNY_MONO
                Internal_GetLocalEulerAngles(m_InternalPtr, out Vector3 temp);
                return temp;
#else
                return ManagedRuntimeContext.TransformGetLocalEulerAngles(entity.uuid);
#endif
            }
            set
            {
#if CROWNY_MONO
                Internal_SetLocalEulerAngles(m_InternalPtr, ref value);
#else
                ManagedRuntimeContext.TransformSetLocalEulerAngles(entity.uuid, value);
#endif
            }
        }

        /// <summary>
        /// A matrix that transforms points from world space to local space.
        /// </summary>
        public Matrix4 worldToLocalMatrix
        {
            get
            {
#if CROWNY_MONO
                Internal_GetWorldToLocalTransform(m_InternalPtr, out Matrix4 tmp);
                return tmp;
#else
                return ManagedRuntimeContext.TransformGetWorldToLocalMatrix(entity.uuid);
#endif
            }
        }

        /// <summary>
        /// A matrix that transforms points from local space to world space.
        /// </summary>
        public Matrix4 localToWorldMatrix
        {
            get
            {
#if CROWNY_MONO
                Internal_GetLocalToWorldMatrix(m_InternalPtr, out Matrix4 tmp);
                return tmp;
#else
                return ManagedRuntimeContext.TransformGetLocalToWorldMatrix(entity.uuid);
#endif
            }
        }

        /// <summary>
        /// Returns true if the transform's dirty flag has changed after the last frame.
        /// </summary>
        public bool IsDirty(DirtyFlag dirtyFlag)
        {
#if CROWNY_MONO
            return Internal_IsDirty(m_InternalPtr, dirtyFlag);
#else
            return false;
#endif
        }

#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetPosition(IntPtr thisptr, out Vector3 output);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private  static extern void Internal_SetPosition(IntPtr thisptr, ref Vector3 value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private  static extern void Internal_GetLocalPosition(IntPtr thisptr, out Vector3 output);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private  static extern void Internal_SetLocalPosition(IntPtr thisptr, ref Vector3 value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private  static extern void Internal_GetLocalScale(IntPtr thisptr, out Vector3 output);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private  static extern void Internal_SetLocalScale(IntPtr thisptr, ref Vector3 value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private  static extern void Internal_GetEulerAngles(IntPtr thisptr, out Vector3 output);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private  static extern void Internal_SetEulerAngles(IntPtr thisptr, ref Vector3 value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private  static extern void Internal_GetLocalEulerAngles(IntPtr thisptr, out Vector3 output);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetLocalEulerAngles(IntPtr thisptr, ref Vector3 value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetLocalRotation(IntPtr thisptr, out Quaternion output);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetLocalRotation(IntPtr thisptr, ref Quaternion value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetRotation(IntPtr thisptr, out Quaternion output);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetRotation(IntPtr thisptr, ref Quaternion value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetScale(IntPtr thisptr, out Vector3 output);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetScale(IntPtr thisptr, ref Vector3 value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetWorldToLocalTransform(IntPtr thisptr, out Matrix4 value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetLocalToWorldMatrix(IntPtr thisptr, out Matrix4 value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_IsDirty(IntPtr thisptr, DirtyFlag flag);
#endif

    }
}

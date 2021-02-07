using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    public class Transform : Component
    {
        /// <summary>
        /// The position of the transform in world space.
        /// </summary>
        /// <value>A vector3 world position.</value>
        public Vector3 position
        {
            get
            {
                Vector3 tmp;
                Internal_GetPosition(m_InternalPtr, out tmp);
                return tmp;
            }
            set { Internal_SetPosition(m_InternalPtr, ref value); }
        }

        /// <summary>
        /// The position of the transform in local space.
        /// </summary>
        /// <value>A Vector3 local position.</value>
        public Vector3 localPosition 
        { 
            get 
            { 
                Vector3 temp;
                Internal_GetLocalPosition(m_InternalPtr, out temp);
                return temp;
            } 
            set { Internal_SetLocalPosition(m_InternalPtr, ref value); }
        }

        // Rotation of the transform
        //public extern Quaternion rotation { get; set; }

        /// <summary>
        /// Local scale of the transform.
        /// </summary>
        /// <value>A Vector3 local scale.</value>
        public Vector3 localScale 
        {
            get
            {
                Vector3 temp;
                Internal_GetLocalScale(m_InternalPtr, out temp);
                return temp;
            } 
            set { Internal_SetLocalScale(m_InternalPtr, ref value); } 
        }

        /// <summary>
        /// Euler angle rotation of the transform.
        /// </summary>
        /// <value>A Vector3 world rotation.</value>
        public Vector3 eulerAngles
        {
            get
            { 
                Vector3 temp;
                Internal_GetEulerAngles(m_InternalPtr, out temp);
                return temp;
            } 
            set { Internal_SetEulerAngles(m_InternalPtr, ref value); } 
        }

        /// <summary>
        /// Euler angle local rotation of the transform,
        /// </summary>
        /// <value>A Vector3 local rotation.</value>
        public Vector3 localEulerAngles
        {
            get
            {
                Vector3 temp;
                Internal_GetLocalEulerAngles(m_InternalPtr, out temp);
                return temp;
            } 
            set { Internal_SetEulerAngles(m_InternalPtr, ref value); } 
        }

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
        private  static extern void Internal_SetLocalEulerAngles(IntPtr thisptr, ref Vector3 value);

    }
}

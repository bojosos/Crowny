using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    public enum CameraProjection
    {
        Orthographic,
        Perspective
    }

    public class Camera : ScriptObject
    {
        // Gets the field of view of the camera
        public float fieldOfView
        {
            get { return Internal_GetCameraFov(m_InternalPtr); }
            set { Internal_SetCameraFov(m_InternalPtr, value); }
        }

        public float nearClipPlane
        {
            get { return Internal_GetCameraNearPlane(m_InternalPtr); }
            set { Internal_SetCameraNearPlane(m_InternalPtr, value); }
        }

        public float farClipPlane
        {
            get { return Internal_GetCameraFarPlane(m_InternalPtr); }
            set { Internal_SetCameraFarPlane(m_InternalPtr, value); }
        }

        public float orthographicSize
        {
            get { return Internal_GetCameraOrthographicSize(m_InternalPtr); }
            set { Internal_SetCameraOrthographicSize(m_InternalPtr, value); }
        }

        public float aspectRatio
        {
            get { return Internal_GetCameraAspectRatio(m_InternalPtr); }
            set { Internal_SetCameraAspectRatio(m_InternalPtr, value); }
        }

        public Vector3 backgroundColor
        {
            get
            {
                Vector3 temp;
                Internal_GetCameraBackgroundColor(m_InternalPtr, out temp);
                return temp;
            }
            set { Internal_SetCameraBackgroundColor(m_InternalPtr, ref value); }
        }

		public Vector4 viewportRectangle
        {
            get
            {
                Vector4 temp;
                Internal_GetCameraViewportRectangle(m_InternalPtr, out temp);
                return temp;
            }
            set { Internal_SetCameraViewportRectangle(m_InternalPtr, ref value); }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetCameraFov(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetCameraFov(IntPtr thisptr, float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetCameraNearPlane(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetCameraNearPlane(IntPtr thisptr, float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetCameraFarPlane(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetCameraFarPlane(IntPtr thisptr, float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetCameraOrthographicSize(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetCameraOrthographicSize(IntPtr thisptr, float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetCameraAspectRatio(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetCameraAspectRatio(IntPtr thisptr, float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetCameraBackgroundColor(IntPtr thisptr, out Vector3 value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetCameraBackgroundColor(IntPtr thisptr, ref Vector3 value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetCameraViewportRectangle(IntPtr thisptr, out Vector4 value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetCameraViewportRectangle(IntPtr thisptr, ref Vector4 value);
    }
}
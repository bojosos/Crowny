using System;
using System.Runtime.CompilerServices;

namespace Crowny
{

    public class Material : Asset
    {
        public void SetFloat(string name, float value) => Internal_SetFloat(m_InternalPtr, name, value);
        public void SetVector2(string name, Vector2 value) => Internal_SetFloat2(m_InternalPtr, name, ref value);
        public void SetInt(string name, int value) => Internal_SetInt(m_InternalPtr, name, value);
        public void SetColor(string name, Color value) => Internal_SetColor(m_InternalPtr, name, ref value);
        public void SetVector3(string name, Vector3 value) => Internal_SetVector3(m_InternalPtr, name, ref value);
        public void SetMatrix(string name, Matrix4 value) => Internal_SetMatrix(m_InternalPtr, name, ref value);
        public void SetTexture(string name, Texture value) => Internal_SetTexture(m_InternalPtr, name, value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetFloat(IntPtr thisPtr, string name, float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetFloat2(IntPtr thisPtr, string name, ref Vector2 value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetInt(IntPtr thisPtr, string name, int value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetColor(IntPtr thisPtr, string name, ref Color value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetVector3(IntPtr thisPtr, string name, ref Vector3 value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetMatrix(IntPtr thisPtr, string name, ref Matrix4 value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetTexture(IntPtr thisPtr, string name, Texture texture);
    }
}

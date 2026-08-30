using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    /// <summary>Controls how a material contributes to the scene color and transparency passes.</summary>
    public enum AlphaMode
    {
        Opaque = 0,
        Mask = 1,
        Premultiplied = 2,
        Additive = 3,
        WeightedOIT = 4
    }

    public class Material : Asset
    {
        /// <summary>
        /// Gets or sets the explicit alpha routing override. A null value keeps shader-based inference.
        /// </summary>
        public AlphaMode? AlphaModeOverride
        {
            get => Internal_HasAlphaModeOverride(m_InternalPtr)
                ? (AlphaMode)Internal_GetAlphaMode(m_InternalPtr)
                : (AlphaMode?)null;
            set
            {
                if (value.HasValue)
                    Internal_SetAlphaMode(m_InternalPtr, (int)value.Value);
                else
                    Internal_ClearAlphaModeOverride(m_InternalPtr);
            }
        }

        /// <summary>Returns true when this material overrides shader-based alpha routing.</summary>
        public bool HasAlphaModeOverride => Internal_HasAlphaModeOverride(m_InternalPtr);

        /// <summary>Restores shader-based alpha routing.</summary>
        public void ClearAlphaModeOverride() => Internal_ClearAlphaModeOverride(m_InternalPtr);

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
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_HasAlphaModeOverride(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int Internal_GetAlphaMode(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetAlphaMode(IntPtr thisPtr, int alphaMode);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_ClearAlphaModeOverride(IntPtr thisPtr);
    }
}

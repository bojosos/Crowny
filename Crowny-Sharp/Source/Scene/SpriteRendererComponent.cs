using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    public class SpriteRendererComponent : Component
    {
        public Texture Texture
        {
            get { return Internal_GetTexture(m_InternalPtr); }
            set { Internal_SetTexture(m_InternalPtr, value); }
        }

        public Color Color
        {
            get { Internal_GetColor(m_InternalPtr, out Color value); return value; }
            set { Internal_SetColor(m_InternalPtr, ref value); }
        }

        /// <summary>Primary stable ordering key shared by sprites and text.</summary>
        public int SortingLayer
        {
            get { return Internal_GetSortingLayer(m_InternalPtr); }
            set { Internal_SetSortingLayer(m_InternalPtr, value); }
        }

        /// <summary>Ordering key within SortingLayer. Lower values render first.</summary>
        public int OrderInLayer
        {
            get { return Internal_GetOrderInLayer(m_InternalPtr); }
            set { Internal_SetOrderInLayer(m_InternalPtr, value); }
        }

        [Obsolete("Use Texture instead.")]
        public Texture texture { get { return Texture; } set { Texture = value; } }
        [Obsolete("Use Color instead.")]
        public Color color { get { return Color; } set { Color = value; } }
        [Obsolete("Use SortingLayer instead.")]
        public int sortingLayer { get { return SortingLayer; } set { SortingLayer = value; } }
        [Obsolete("Use OrderInLayer instead.")]
        public int orderInLayer { get { return OrderInLayer; } set { OrderInLayer = value; } }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Texture Internal_GetTexture(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetTexture(IntPtr thisPtr, Texture value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetColor(IntPtr thisPtr, out Color value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetColor(IntPtr thisPtr, ref Color value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int Internal_GetSortingLayer(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetSortingLayer(IntPtr thisPtr, int value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int Internal_GetOrderInLayer(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetOrderInLayer(IntPtr thisPtr, int value);
    }
}

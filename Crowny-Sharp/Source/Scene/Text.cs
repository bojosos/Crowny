using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    public enum TextOverflow
    {
        Overflow,
        Ellipses,
        Truncate
    };

    public enum TextHorizontalAlignment
    {
        Left,
        Center,
        Right,
        Justified,
        Flush
    };

    public enum TextVerticalAlignment
    {
        Top,
        Middle,
        Bottom,
        Baseline,
        Midline
    };

    public enum FontStyle
    {
        None = 0,
        Bold = 1 << 0,
        Italic = 1 << 1,
        Underline = 1 << 2,
        Strikethrough = 1 << 3
    };

    public class Text : Component
    {
        public string text
        {
            get { return Internal_GetText(m_InternalPtr); }
            set { Internal_SetText(m_InternalPtr, value); }
        }

        Font font
        {
            get { return Internal_GetFont(m_InternalPtr); }
            set { Internal_SetFont(m_InternalPtr, value); }
        }

        Color color
        {
            get { Internal_GetColor(m_InternalPtr, out Color color); return color; }
            set { Internal_SetColor(m_InternalPtr, ref value); }
        }

        float size;
        bool autoSize;
        bool wrapping;
        TextOverflow Overflow;
        TextHorizontalAlignment HorizontalAlignment;
        TextVerticalAlignment VerticalAlignment;
        FontStyle FontStyle;

        Color OutlineColor
        {
            get { Internal_GetOutlineColor(m_InternalPtr, out Color color); return color; }
            set { Internal_SetOutlineColor(m_InternalPtr, ref value); }

        }
        float Thickess;

        float CharacterSpacing;
        float WordSpacing;
        float LineSpacing;
        bool UseKerning;

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string Internal_GetText(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetText(IntPtr thisptr, string value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Font Internal_GetFont(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetFont(IntPtr thisptr, Font value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string Internal_GetColor(IntPtr thisptr, out Color color);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetColor(IntPtr thisptr, ref Color color);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string Internal_GetOutlineColor(IntPtr thisptr, out Color color);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetOutlineColor(IntPtr thisptr, ref Color color);
    }
}
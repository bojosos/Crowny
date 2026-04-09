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

        public Font font
        {
            get { return Internal_GetFont(m_InternalPtr); }
            set { Internal_SetFont(m_InternalPtr, value); }
        }

        public Color color
        {
            get { Internal_GetColor(m_InternalPtr, out Color color); return color; }
            set { Internal_SetColor(m_InternalPtr, ref value); }
        }

        public float size
        {
            get { return Internal_GetSize(m_InternalPtr); }
            set { Internal_SetSize(m_InternalPtr, value); }
        }

        public bool autoSize
        {
            get { return Internal_GetAutoSize(m_InternalPtr); }
            set { Internal_SetAutoSize(m_InternalPtr, value); }
        }

        public bool wrapping
        {
            get { return Internal_GetWrapping(m_InternalPtr); }
            set { Internal_SetWrapping(m_InternalPtr, value); }
        }

        public TextOverflow overflow
        {
            get { return Internal_GetOverflow(m_InternalPtr); }
            set { Internal_SetOverflow(m_InternalPtr, value); }
        }

        public TextHorizontalAlignment horizontalAlignment
        {
            get { return Internal_GetHorizontalAlignment(m_InternalPtr); }
            set { Internal_SetHorizontalAlignment(m_InternalPtr, value); }
        }

        public TextVerticalAlignment verticalAlignment
        {
            get { return Internal_GetVerticalAlignment(m_InternalPtr); }
            set { Internal_SetVerticalAlignment(m_InternalPtr, value); }
        }

        public FontStyle fontStyle
        {
            get { return Internal_GetFontStyle(m_InternalPtr); }
            set { Internal_SetFontStyle(m_InternalPtr, value); }
        }

        public Color outlineColor
        {
            get { Internal_GetOutlineColor(m_InternalPtr, out Color color); return color; }
            set { Internal_SetOutlineColor(m_InternalPtr, ref value); }
        }

        public float thickness
        {
            get { return Internal_GetThickness(m_InternalPtr); }
            set { Internal_SetThickness(m_InternalPtr, value); }
        }

        public float characterSpacing
        {
            get { return Internal_GetCharacterSpacing(m_InternalPtr); }
            set { Internal_SetCharacterSpacing(m_InternalPtr, value); }
        }

        public float wordSpacing
        {
            get { return Internal_GetWordSpacing(m_InternalPtr); }
            set { Internal_SetWordSpacing(m_InternalPtr, value); }
        }

        public float lineSpacing
        {
            get { return Internal_GetLineSpacing(m_InternalPtr); }
            set { Internal_SetLineSpacing(m_InternalPtr, value); }
        }

        public bool useKerning
        {
            get { return Internal_GetUseKerning(m_InternalPtr); }
            set { Internal_SetUseKerning(m_InternalPtr, value); }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string Internal_GetText(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetText(IntPtr thisptr, string value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Font Internal_GetFont(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetFont(IntPtr thisptr, Font value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetColor(IntPtr thisptr, out Color color);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetColor(IntPtr thisptr, ref Color color);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetOutlineColor(IntPtr thisptr, out Color color);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetOutlineColor(IntPtr thisptr, ref Color color);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetSize(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetSize(IntPtr thisptr, float size);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_GetAutoSize(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetAutoSize(IntPtr thisptr, bool autoSize);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_GetWrapping(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetWrapping(IntPtr thisptr, bool wrapping);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern TextOverflow Internal_GetOverflow(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetOverflow(IntPtr thisptr, TextOverflow overflow);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern TextHorizontalAlignment Internal_GetHorizontalAlignment(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetHorizontalAlignment(IntPtr thisptr, TextHorizontalAlignment alignment);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern TextVerticalAlignment Internal_GetVerticalAlignment(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetVerticalAlignment(IntPtr thisptr, TextVerticalAlignment alignment);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern FontStyle Internal_GetFontStyle(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetFontStyle(IntPtr thisptr, FontStyle style);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetThickness(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetThickness(IntPtr thisptr, float thickness);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetCharacterSpacing(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetCharacterSpacing(IntPtr thisptr, float spacing);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetWordSpacing(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetWordSpacing(IntPtr thisptr, float spacing);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetLineSpacing(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetLineSpacing(IntPtr thisptr, float spacing);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_GetUseKerning(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetUseKerning(IntPtr thisptr, bool useKerning);
    }
}

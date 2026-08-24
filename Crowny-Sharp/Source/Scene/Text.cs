using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    public enum TextOverflow
    {
        Overflow,
        Ellipses,
        Ellipsis = Ellipses,
        Truncate
    };

    /// <summary>Chooses where bounded text may create soft line breaks.</summary>
    public enum TextWrapMode
    {
        Word,
        Character,
        WordThenCharacter
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

        /// <summary>Smallest size considered by automatic fitting.</summary>
        public float autoSizeMin
        {
            get { return Internal_GetAutoSizeMin(m_InternalPtr); }
            set { Internal_SetAutoSizeMin(m_InternalPtr, value); }
        }

        /// <summary>Largest size considered by automatic fitting.</summary>
        public float autoSizeMax
        {
            get { return Internal_GetAutoSizeMax(m_InternalPtr); }
            set { Internal_SetAutoSizeMax(m_InternalPtr, value); }
        }

        /// <summary>Layout box extending right and down from the entity origin. A zero axis is unbounded.</summary>
        public Vector2 layoutSize
        {
            get { Internal_GetLayoutSize(m_InternalPtr, out Vector2 value); return value; }
            set { Internal_SetLayoutSize(m_InternalPtr, ref value); }
        }

        public bool wrapping
        {
            get { return Internal_GetWrapping(m_InternalPtr); }
            set { Internal_SetWrapping(m_InternalPtr, value); }
        }

        /// <summary>Controls soft line breaks when wrapping is enabled.</summary>
        public TextWrapMode wrapMode
        {
            get { return Internal_GetWrapMode(m_InternalPtr); }
            set { Internal_SetWrapMode(m_InternalPtr, value); }
        }

        public TextOverflow overflow
        {
            get { return Internal_GetOverflow(m_InternalPtr); }
            set { Internal_SetOverflow(m_InternalPtr, value); }
        }

        /// <summary>Clips glyphs and decorations at the layout box edges.</summary>
        public bool clipToBounds
        {
            get { return Internal_GetClipToBounds(m_InternalPtr); }
            set { Internal_SetClipToBounds(m_InternalPtr, value); }
        }

        /// <summary>Maximum laid-out lines. Zero allows any number of lines.</summary>
        public uint maxLines
        {
            get { return Internal_GetMaxLines(m_InternalPtr); }
            set { Internal_SetMaxLines(m_InternalPtr, value); }
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

        public float outlineWidth
        {
            get { return Internal_GetThickness(m_InternalPtr); }
            set { Internal_SetThickness(m_InternalPtr, value); }
        }

        [Obsolete("Use outlineWidth instead.")]
        public float thickness
        {
            get { return outlineWidth; }
            set { outlineWidth = value; }
        }

        public float characterSpacing
        {
            get { return Internal_GetCharacterSpacing(m_InternalPtr); }
            set { Internal_SetCharacterSpacing(m_InternalPtr, value); }
        }

        /// <summary>Alias for characterSpacing.</summary>
        public float letterSpacing
        {
            get { return characterSpacing; }
            set { characterSpacing = value; }
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

        /// <summary>Extra distance after an explicit newline. Soft wraps use lineSpacing.</summary>
        public float paragraphSpacing
        {
            get { return Internal_GetParagraphSpacing(m_InternalPtr); }
            set { Internal_SetParagraphSpacing(m_InternalPtr, value); }
        }

        /// <summary>Uses decorationColor instead of the text color for underline and strikethrough.</summary>
        public bool useCustomDecorationColor
        {
            get { return Internal_GetUseCustomDecorationColor(m_InternalPtr); }
            set { Internal_SetUseCustomDecorationColor(m_InternalPtr, value); }
        }

        /// <summary>Underline and strikethrough color when useCustomDecorationColor is enabled.</summary>
        public Color decorationColor
        {
            get { Internal_GetDecorationColor(m_InternalPtr, out Color color); return color; }
            set { Internal_SetDecorationColor(m_InternalPtr, ref value); }
        }

        /// <summary>Decoration line width. Zero uses the font metric.</summary>
        public float decorationThickness
        {
            get { return Internal_GetDecorationThickness(m_InternalPtr); }
            set { Internal_SetDecorationThickness(m_InternalPtr, value); }
        }

        /// <summary>Offset added to the font-derived underline position.</summary>
        public float underlineOffset
        {
            get { return Internal_GetUnderlineOffset(m_InternalPtr); }
            set { Internal_SetUnderlineOffset(m_InternalPtr, value); }
        }

        /// <summary>Offset added to the font-derived strikethrough position.</summary>
        public float strikethroughOffset
        {
            get { return Internal_GetStrikethroughOffset(m_InternalPtr); }
            set { Internal_SetStrikethroughOffset(m_InternalPtr, value); }
        }

        public bool useKerning
        {
            get { return Internal_GetUseKerning(m_InternalPtr); }
            set { Internal_SetUseKerning(m_InternalPtr, value); }
        }

        /// <summary>Primary stable ordering key shared by text and sprites.</summary>
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

        [Obsolete("Use SortingLayer instead.")]
        public int sortingLayer { get { return SortingLayer; } set { SortingLayer = value; } }

        [Obsolete("Use OrderInLayer instead.")]
        public int orderInLayer { get { return OrderInLayer; } set { OrderInLayer = value; } }

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
        private static extern float Internal_GetAutoSizeMin(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetAutoSizeMin(IntPtr thisptr, float size);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetAutoSizeMax(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetAutoSizeMax(IntPtr thisptr, float size);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetLayoutSize(IntPtr thisptr, out Vector2 size);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetLayoutSize(IntPtr thisptr, ref Vector2 size);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_GetWrapping(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetWrapping(IntPtr thisptr, bool wrapping);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern TextWrapMode Internal_GetWrapMode(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetWrapMode(IntPtr thisptr, TextWrapMode mode);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern TextOverflow Internal_GetOverflow(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetOverflow(IntPtr thisptr, TextOverflow overflow);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_GetClipToBounds(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetClipToBounds(IntPtr thisptr, bool clip);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern uint Internal_GetMaxLines(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetMaxLines(IntPtr thisptr, uint maxLines);
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
        private static extern float Internal_GetParagraphSpacing(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetParagraphSpacing(IntPtr thisptr, float spacing);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_GetUseCustomDecorationColor(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetUseCustomDecorationColor(IntPtr thisptr, bool useCustomColor);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetDecorationColor(IntPtr thisptr, out Color color);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetDecorationColor(IntPtr thisptr, ref Color color);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetDecorationThickness(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetDecorationThickness(IntPtr thisptr, float thickness);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetUnderlineOffset(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetUnderlineOffset(IntPtr thisptr, float offset);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetStrikethroughOffset(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetStrikethroughOffset(IntPtr thisptr, float offset);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_GetUseKerning(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetUseKerning(IntPtr thisptr, bool useKerning);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int Internal_GetSortingLayer(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetSortingLayer(IntPtr thisptr, int value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int Internal_GetOrderInLayer(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetOrderInLayer(IntPtr thisptr, int value);
    }
}

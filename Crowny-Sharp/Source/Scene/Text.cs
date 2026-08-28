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
            get { return GetText(); }
            set { SetText(value); }
        }

        public Font font
        {
            get { return GetFont(); }
            set { SetFont(value); }
        }

        public Color color
        {
            get { return GetColor(); }
            set { SetColor(value); }
        }

        public float size
        {
            get { return GetSize(); }
            set { SetSize(value); }
        }

        public bool autoSize
        {
            get { return GetAutoSize(); }
            set { SetAutoSize(value); }
        }

        /// <summary>Smallest size considered by automatic fitting.</summary>
        public float autoSizeMin
        {
            get { return GetAutoSizeMin(); }
            set { SetAutoSizeMin(value); }
        }

        /// <summary>Largest size considered by automatic fitting.</summary>
        public float autoSizeMax
        {
            get { return GetAutoSizeMax(); }
            set { SetAutoSizeMax(value); }
        }

        /// <summary>Layout box extending right and down from the entity origin. A zero axis is unbounded.</summary>
        public Vector2 layoutSize
        {
            get { return GetLayoutSize(); }
            set { SetLayoutSize(value); }
        }

        public bool wrapping
        {
            get { return GetWrapping(); }
            set { SetWrapping(value); }
        }

        /// <summary>Controls soft line breaks when wrapping is enabled.</summary>
        public TextWrapMode wrapMode
        {
            get { return GetWrapMode(); }
            set { SetWrapMode(value); }
        }

        public TextOverflow overflow
        {
            get { return GetOverflow(); }
            set { SetOverflow(value); }
        }

        /// <summary>Clips glyphs and decorations at the layout box edges.</summary>
        public bool clipToBounds
        {
            get { return GetClipToBounds(); }
            set { SetClipToBounds(value); }
        }

        /// <summary>Maximum laid-out lines. Zero allows any number of lines.</summary>
        public uint maxLines
        {
            get { return GetMaxLines(); }
            set { SetMaxLines(value); }
        }

        public TextHorizontalAlignment horizontalAlignment
        {
            get { return GetHorizontalAlignment(); }
            set { SetHorizontalAlignment(value); }
        }

        public TextVerticalAlignment verticalAlignment
        {
            get { return GetVerticalAlignment(); }
            set { SetVerticalAlignment(value); }
        }

        public FontStyle fontStyle
        {
            get { return GetFontStyle(); }
            set { SetFontStyle(value); }
        }

        public Color outlineColor
        {
            get { return GetOutlineColor(); }
            set { SetOutlineColor(value); }
        }

        public float outlineWidth
        {
            get { return GetOutlineWidth(); }
            set { SetOutlineWidth(value); }
        }

        [Obsolete("Use outlineWidth instead.")]
        public float thickness
        {
            get { return outlineWidth; }
            set { outlineWidth = value; }
        }

        /// <summary>Color of the text shadow. A transparent color disables the shadow.</summary>
        public Color shadowColor
        {
            get { return GetShadowColor(); }
            set { SetShadowColor(value); }
        }

        /// <summary>Shadow displacement from the glyph origin.</summary>
        public Vector2 shadowOffset
        {
            get { return GetShadowOffset(); }
            set { SetShadowOffset(value); }
        }

        /// <summary>Additional edge softness applied to the text shadow.</summary>
        public float shadowSoftness
        {
            get { return GetShadowSoftness(); }
            set { SetShadowSoftness(value); }
        }

        public float characterSpacing
        {
            get { return GetCharacterSpacing(); }
            set { SetCharacterSpacing(value); }
        }

        /// <summary>Alias for characterSpacing.</summary>
        public float letterSpacing
        {
            get { return characterSpacing; }
            set { characterSpacing = value; }
        }

        public float wordSpacing
        {
            get { return GetWordSpacing(); }
            set { SetWordSpacing(value); }
        }

        public float lineSpacing
        {
            get { return GetLineSpacing(); }
            set { SetLineSpacing(value); }
        }

        /// <summary>Extra distance after an explicit newline. Soft wraps use lineSpacing.</summary>
        public float paragraphSpacing
        {
            get { return GetParagraphSpacing(); }
            set { SetParagraphSpacing(value); }
        }

        /// <summary>Number of space advances used by a tab. Values below one are clamped.</summary>
        public uint tabWidth
        {
            get { return GetTabWidth(); }
            set { SetTabWidth(value); }
        }

        /// <summary>Uses decorationColor instead of the text color for underline and strikethrough.</summary>
        public bool useCustomDecorationColor
        {
            get { return GetUseCustomDecorationColor(); }
            set { SetUseCustomDecorationColor(value); }
        }

        /// <summary>Underline and strikethrough color when useCustomDecorationColor is enabled.</summary>
        public Color decorationColor
        {
            get { return GetDecorationColor(); }
            set { SetDecorationColor(value); }
        }

        /// <summary>Decoration line width. Zero uses the font metric.</summary>
        public float decorationThickness
        {
            get { return GetDecorationThickness(); }
            set { SetDecorationThickness(value); }
        }

        /// <summary>Offset added to the font-derived underline position.</summary>
        public float underlineOffset
        {
            get { return GetUnderlineOffset(); }
            set { SetUnderlineOffset(value); }
        }

        /// <summary>Offset added to the font-derived strikethrough position.</summary>
        public float strikethroughOffset
        {
            get { return GetStrikethroughOffset(); }
            set { SetStrikethroughOffset(value); }
        }

        public bool useKerning
        {
            get { return GetUseKerning(); }
            set { SetUseKerning(value); }
        }

        /// <summary>Primary stable ordering key shared by text and sprites.</summary>
        public int SortingLayer
        {
            get { return GetSortingLayer(); }
            set { SetSortingLayer(value); }
        }

        /// <summary>Ordering key within SortingLayer. Lower values render first.</summary>
        public int OrderInLayer
        {
            get { return GetOrderInLayer(); }
            set { SetOrderInLayer(value); }
        }

        [Obsolete("Use SortingLayer instead.")]
        public int sortingLayer { get { return SortingLayer; } set { SortingLayer = value; } }

        [Obsolete("Use OrderInLayer instead.")]
        public int orderInLayer { get { return OrderInLayer; } set { OrderInLayer = value; } }

#if CROWNY_MONO
        private string GetText() => Internal_GetText(m_InternalPtr);
        private void SetText(string value) => Internal_SetText(m_InternalPtr, value);
        private Font GetFont() => Internal_GetFont(m_InternalPtr);
        private void SetFont(Font value) => Internal_SetFont(m_InternalPtr, value);
        private Color GetColor() { Internal_GetColor(m_InternalPtr, out Color value); return value; }
        private void SetColor(Color value) => Internal_SetColor(m_InternalPtr, ref value);
        private float GetSize() => Internal_GetSize(m_InternalPtr);
        private void SetSize(float value) => Internal_SetSize(m_InternalPtr, value);
        private bool GetAutoSize() => Internal_GetAutoSize(m_InternalPtr);
        private void SetAutoSize(bool value) => Internal_SetAutoSize(m_InternalPtr, value);
        private float GetAutoSizeMin() => Internal_GetAutoSizeMin(m_InternalPtr);
        private void SetAutoSizeMin(float value) => Internal_SetAutoSizeMin(m_InternalPtr, value);
        private float GetAutoSizeMax() => Internal_GetAutoSizeMax(m_InternalPtr);
        private void SetAutoSizeMax(float value) => Internal_SetAutoSizeMax(m_InternalPtr, value);
        private Vector2 GetLayoutSize() { Internal_GetLayoutSize(m_InternalPtr, out Vector2 value); return value; }
        private void SetLayoutSize(Vector2 value) => Internal_SetLayoutSize(m_InternalPtr, ref value);
        private bool GetWrapping() => Internal_GetWrapping(m_InternalPtr);
        private void SetWrapping(bool value) => Internal_SetWrapping(m_InternalPtr, value);
        private TextWrapMode GetWrapMode() => Internal_GetWrapMode(m_InternalPtr);
        private void SetWrapMode(TextWrapMode value) => Internal_SetWrapMode(m_InternalPtr, value);
        private TextOverflow GetOverflow() => Internal_GetOverflow(m_InternalPtr);
        private void SetOverflow(TextOverflow value) => Internal_SetOverflow(m_InternalPtr, value);
        private bool GetClipToBounds() => Internal_GetClipToBounds(m_InternalPtr);
        private void SetClipToBounds(bool value) => Internal_SetClipToBounds(m_InternalPtr, value);
        private uint GetMaxLines() => Internal_GetMaxLines(m_InternalPtr);
        private void SetMaxLines(uint value) => Internal_SetMaxLines(m_InternalPtr, value);
        private TextHorizontalAlignment GetHorizontalAlignment() => Internal_GetHorizontalAlignment(m_InternalPtr);
        private void SetHorizontalAlignment(TextHorizontalAlignment value) => Internal_SetHorizontalAlignment(m_InternalPtr, value);
        private TextVerticalAlignment GetVerticalAlignment() => Internal_GetVerticalAlignment(m_InternalPtr);
        private void SetVerticalAlignment(TextVerticalAlignment value) => Internal_SetVerticalAlignment(m_InternalPtr, value);
        private FontStyle GetFontStyle() => Internal_GetFontStyle(m_InternalPtr);
        private void SetFontStyle(FontStyle value) => Internal_SetFontStyle(m_InternalPtr, value);
        private Color GetOutlineColor() { Internal_GetOutlineColor(m_InternalPtr, out Color value); return value; }
        private void SetOutlineColor(Color value) => Internal_SetOutlineColor(m_InternalPtr, ref value);
        private float GetOutlineWidth() => Internal_GetThickness(m_InternalPtr);
        private void SetOutlineWidth(float value) => Internal_SetThickness(m_InternalPtr, value);
        private Color GetShadowColor() { Internal_GetShadowColor(m_InternalPtr, out Color value); return value; }
        private void SetShadowColor(Color value) => Internal_SetShadowColor(m_InternalPtr, ref value);
        private Vector2 GetShadowOffset() { Internal_GetShadowOffset(m_InternalPtr, out Vector2 value); return value; }
        private void SetShadowOffset(Vector2 value) => Internal_SetShadowOffset(m_InternalPtr, ref value);
        private float GetShadowSoftness() => Internal_GetShadowSoftness(m_InternalPtr);
        private void SetShadowSoftness(float value) => Internal_SetShadowSoftness(m_InternalPtr, value);
        private float GetCharacterSpacing() => Internal_GetCharacterSpacing(m_InternalPtr);
        private void SetCharacterSpacing(float value) => Internal_SetCharacterSpacing(m_InternalPtr, value);
        private float GetWordSpacing() => Internal_GetWordSpacing(m_InternalPtr);
        private void SetWordSpacing(float value) => Internal_SetWordSpacing(m_InternalPtr, value);
        private float GetLineSpacing() => Internal_GetLineSpacing(m_InternalPtr);
        private void SetLineSpacing(float value) => Internal_SetLineSpacing(m_InternalPtr, value);
        private float GetParagraphSpacing() => Internal_GetParagraphSpacing(m_InternalPtr);
        private void SetParagraphSpacing(float value) => Internal_SetParagraphSpacing(m_InternalPtr, value);
        private uint GetTabWidth() => Internal_GetTabWidth(m_InternalPtr);
        private void SetTabWidth(uint value) => Internal_SetTabWidth(m_InternalPtr, value);
        private bool GetUseCustomDecorationColor() => Internal_GetUseCustomDecorationColor(m_InternalPtr);
        private void SetUseCustomDecorationColor(bool value) => Internal_SetUseCustomDecorationColor(m_InternalPtr, value);
        private Color GetDecorationColor() { Internal_GetDecorationColor(m_InternalPtr, out Color value); return value; }
        private void SetDecorationColor(Color value) => Internal_SetDecorationColor(m_InternalPtr, ref value);
        private float GetDecorationThickness() => Internal_GetDecorationThickness(m_InternalPtr);
        private void SetDecorationThickness(float value) => Internal_SetDecorationThickness(m_InternalPtr, value);
        private float GetUnderlineOffset() => Internal_GetUnderlineOffset(m_InternalPtr);
        private void SetUnderlineOffset(float value) => Internal_SetUnderlineOffset(m_InternalPtr, value);
        private float GetStrikethroughOffset() => Internal_GetStrikethroughOffset(m_InternalPtr);
        private void SetStrikethroughOffset(float value) => Internal_SetStrikethroughOffset(m_InternalPtr, value);
        private bool GetUseKerning() => Internal_GetUseKerning(m_InternalPtr);
        private void SetUseKerning(bool value) => Internal_SetUseKerning(m_InternalPtr, value);
        private int GetSortingLayer() => Internal_GetSortingLayer(m_InternalPtr);
        private void SetSortingLayer(int value) => Internal_SetSortingLayer(m_InternalPtr, value);
        private int GetOrderInLayer() => Internal_GetOrderInLayer(m_InternalPtr);
        private void SetOrderInLayer(int value) => Internal_SetOrderInLayer(m_InternalPtr, value);

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
        private static extern void Internal_GetShadowColor(IntPtr thisptr, out Color color);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetShadowColor(IntPtr thisptr, ref Color color);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetShadowOffset(IntPtr thisptr, out Vector2 offset);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetShadowOffset(IntPtr thisptr, ref Vector2 offset);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetShadowSoftness(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetShadowSoftness(IntPtr thisptr, float softness);
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
        private static extern uint Internal_GetTabWidth(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetTabWidth(IntPtr thisptr, uint width);
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
#else
        private UUID EntityId => entity.uuid;
        private string GetText() => ManagedRuntimeContext.TextGetText(EntityId);
        private void SetText(string value) => ManagedRuntimeContext.TextSetText(EntityId, value);
        private Font GetFont() => ManagedRuntimeContext.CreateAsset<Font>(ManagedRuntimeContext.TextGetFont(EntityId));
        private void SetFont(Font value) => ManagedRuntimeContext.TextSetFont(EntityId, value?.uuid ?? UUID.Empty);
        private Color GetColor() => ManagedRuntimeContext.TextGetColor(EntityId);
        private void SetColor(Color value) => ManagedRuntimeContext.TextSetColor(EntityId, value);
        private float GetSize() => ManagedRuntimeContext.TextGetSize(EntityId);
        private void SetSize(float value) => ManagedRuntimeContext.TextSetSize(EntityId, value);
        private bool GetAutoSize() => ManagedRuntimeContext.TextGetAutoSize(EntityId);
        private void SetAutoSize(bool value) => ManagedRuntimeContext.TextSetAutoSize(EntityId, value);
        private float GetAutoSizeMin() => ManagedRuntimeContext.TextGetAutoSizeMin(EntityId);
        private void SetAutoSizeMin(float value) => ManagedRuntimeContext.TextSetAutoSizeMin(EntityId, value);
        private float GetAutoSizeMax() => ManagedRuntimeContext.TextGetAutoSizeMax(EntityId);
        private void SetAutoSizeMax(float value) => ManagedRuntimeContext.TextSetAutoSizeMax(EntityId, value);
        private Vector2 GetLayoutSize() => ManagedRuntimeContext.TextGetLayoutSize(EntityId);
        private void SetLayoutSize(Vector2 value) => ManagedRuntimeContext.TextSetLayoutSize(EntityId, value);
        private bool GetWrapping() => ManagedRuntimeContext.TextGetWrapping(EntityId);
        private void SetWrapping(bool value) => ManagedRuntimeContext.TextSetWrapping(EntityId, value);
        private TextWrapMode GetWrapMode() => (TextWrapMode)ManagedRuntimeContext.TextGetWrapMode(EntityId);
        private void SetWrapMode(TextWrapMode value) => ManagedRuntimeContext.TextSetWrapMode(EntityId, (int)value);
        private TextOverflow GetOverflow() => (TextOverflow)ManagedRuntimeContext.TextGetOverflow(EntityId);
        private void SetOverflow(TextOverflow value) => ManagedRuntimeContext.TextSetOverflow(EntityId, (int)value);
        private bool GetClipToBounds() => ManagedRuntimeContext.TextGetClipToBounds(EntityId);
        private void SetClipToBounds(bool value) => ManagedRuntimeContext.TextSetClipToBounds(EntityId, value);
        private uint GetMaxLines() => ManagedRuntimeContext.TextGetMaxLines(EntityId);
        private void SetMaxLines(uint value) => ManagedRuntimeContext.TextSetMaxLines(EntityId, value);
        private TextHorizontalAlignment GetHorizontalAlignment() => (TextHorizontalAlignment)ManagedRuntimeContext.TextGetHorizontalAlignment(EntityId);
        private void SetHorizontalAlignment(TextHorizontalAlignment value) => ManagedRuntimeContext.TextSetHorizontalAlignment(EntityId, (int)value);
        private TextVerticalAlignment GetVerticalAlignment() => (TextVerticalAlignment)ManagedRuntimeContext.TextGetVerticalAlignment(EntityId);
        private void SetVerticalAlignment(TextVerticalAlignment value) => ManagedRuntimeContext.TextSetVerticalAlignment(EntityId, (int)value);
        private FontStyle GetFontStyle() => (FontStyle)ManagedRuntimeContext.TextGetFontStyle(EntityId);
        private void SetFontStyle(FontStyle value) => ManagedRuntimeContext.TextSetFontStyle(EntityId, (uint)value);
        private Color GetOutlineColor() => ManagedRuntimeContext.TextGetOutlineColor(EntityId);
        private void SetOutlineColor(Color value) => ManagedRuntimeContext.TextSetOutlineColor(EntityId, value);
        private float GetOutlineWidth() => ManagedRuntimeContext.TextGetOutlineWidth(EntityId);
        private void SetOutlineWidth(float value) => ManagedRuntimeContext.TextSetOutlineWidth(EntityId, value);
        private Color GetShadowColor() => ManagedRuntimeContext.TextGetShadowColor(EntityId);
        private void SetShadowColor(Color value) => ManagedRuntimeContext.TextSetShadowColor(EntityId, value);
        private Vector2 GetShadowOffset() => ManagedRuntimeContext.TextGetShadowOffset(EntityId);
        private void SetShadowOffset(Vector2 value) => ManagedRuntimeContext.TextSetShadowOffset(EntityId, value);
        private float GetShadowSoftness() => ManagedRuntimeContext.TextGetShadowSoftness(EntityId);
        private void SetShadowSoftness(float value) => ManagedRuntimeContext.TextSetShadowSoftness(EntityId, value);
        private float GetCharacterSpacing() => ManagedRuntimeContext.TextGetCharacterSpacing(EntityId);
        private void SetCharacterSpacing(float value) => ManagedRuntimeContext.TextSetCharacterSpacing(EntityId, value);
        private float GetWordSpacing() => ManagedRuntimeContext.TextGetWordSpacing(EntityId);
        private void SetWordSpacing(float value) => ManagedRuntimeContext.TextSetWordSpacing(EntityId, value);
        private float GetLineSpacing() => ManagedRuntimeContext.TextGetLineSpacing(EntityId);
        private void SetLineSpacing(float value) => ManagedRuntimeContext.TextSetLineSpacing(EntityId, value);
        private float GetParagraphSpacing() => ManagedRuntimeContext.TextGetParagraphSpacing(EntityId);
        private void SetParagraphSpacing(float value) => ManagedRuntimeContext.TextSetParagraphSpacing(EntityId, value);
        private uint GetTabWidth() => ManagedRuntimeContext.TextGetTabWidth(EntityId);
        private void SetTabWidth(uint value) => ManagedRuntimeContext.TextSetTabWidth(EntityId, value);
        private bool GetUseCustomDecorationColor() => ManagedRuntimeContext.TextGetUseCustomDecorationColor(EntityId);
        private void SetUseCustomDecorationColor(bool value) => ManagedRuntimeContext.TextSetUseCustomDecorationColor(EntityId, value);
        private Color GetDecorationColor() => ManagedRuntimeContext.TextGetDecorationColor(EntityId);
        private void SetDecorationColor(Color value) => ManagedRuntimeContext.TextSetDecorationColor(EntityId, value);
        private float GetDecorationThickness() => ManagedRuntimeContext.TextGetDecorationThickness(EntityId);
        private void SetDecorationThickness(float value) => ManagedRuntimeContext.TextSetDecorationThickness(EntityId, value);
        private float GetUnderlineOffset() => ManagedRuntimeContext.TextGetUnderlineOffset(EntityId);
        private void SetUnderlineOffset(float value) => ManagedRuntimeContext.TextSetUnderlineOffset(EntityId, value);
        private float GetStrikethroughOffset() => ManagedRuntimeContext.TextGetStrikethroughOffset(EntityId);
        private void SetStrikethroughOffset(float value) => ManagedRuntimeContext.TextSetStrikethroughOffset(EntityId, value);
        private bool GetUseKerning() => ManagedRuntimeContext.TextGetUseKerning(EntityId);
        private void SetUseKerning(bool value) => ManagedRuntimeContext.TextSetUseKerning(EntityId, value);
        private int GetSortingLayer() => ManagedRuntimeContext.TextGetSortingLayer(EntityId);
        private void SetSortingLayer(int value) => ManagedRuntimeContext.TextSetSortingLayer(EntityId, value);
        private int GetOrderInLayer() => ManagedRuntimeContext.TextGetOrderInLayer(EntityId);
        private void SetOrderInLayer(int value) => ManagedRuntimeContext.TextSetOrderInLayer(EntityId, value);
#endif
    }
}

using System;

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
            get { return ManagedRuntimeContext.TextGetText(EntityId); }
            set { ManagedRuntimeContext.TextSetText(EntityId, value); }
        }

        public Font font
        {
            get { return ManagedRuntimeContext.CreateAsset<Font>(ManagedRuntimeContext.TextGetFont(EntityId)); }
            set { ManagedRuntimeContext.TextSetFont(EntityId, value?.uuid ?? UUID.Empty); }
        }

        public Color color
        {
            get { return ManagedRuntimeContext.TextGetColor(EntityId); }
            set { ManagedRuntimeContext.TextSetColor(EntityId, value); }
        }

        public float size
        {
            get { return ManagedRuntimeContext.TextGetSize(EntityId); }
            set { ManagedRuntimeContext.TextSetSize(EntityId, value); }
        }

        public bool autoSize
        {
            get { return ManagedRuntimeContext.TextGetAutoSize(EntityId); }
            set { ManagedRuntimeContext.TextSetAutoSize(EntityId, value); }
        }

        /// <summary>Smallest size considered by automatic fitting.</summary>
        public float autoSizeMin
        {
            get { return ManagedRuntimeContext.TextGetAutoSizeMin(EntityId); }
            set { ManagedRuntimeContext.TextSetAutoSizeMin(EntityId, value); }
        }

        /// <summary>Largest size considered by automatic fitting.</summary>
        public float autoSizeMax
        {
            get { return ManagedRuntimeContext.TextGetAutoSizeMax(EntityId); }
            set { ManagedRuntimeContext.TextSetAutoSizeMax(EntityId, value); }
        }

        /// <summary>Layout box extending right and down from the entity origin. A zero axis is unbounded.</summary>
        public Vector2 layoutSize
        {
            get { return ManagedRuntimeContext.TextGetLayoutSize(EntityId); }
            set { ManagedRuntimeContext.TextSetLayoutSize(EntityId, value); }
        }

        public bool wrapping
        {
            get { return ManagedRuntimeContext.TextGetWrapping(EntityId); }
            set { ManagedRuntimeContext.TextSetWrapping(EntityId, value); }
        }

        /// <summary>Controls soft line breaks when wrapping is enabled.</summary>
        public TextWrapMode wrapMode
        {
            get { return (TextWrapMode)ManagedRuntimeContext.TextGetWrapMode(EntityId); }
            set { ManagedRuntimeContext.TextSetWrapMode(EntityId, (int)value); }
        }

        public TextOverflow overflow
        {
            get { return (TextOverflow)ManagedRuntimeContext.TextGetOverflow(EntityId); }
            set { ManagedRuntimeContext.TextSetOverflow(EntityId, (int)value); }
        }

        /// <summary>Clips glyphs and decorations at the layout box edges.</summary>
        public bool clipToBounds
        {
            get { return ManagedRuntimeContext.TextGetClipToBounds(EntityId); }
            set { ManagedRuntimeContext.TextSetClipToBounds(EntityId, value); }
        }

        /// <summary>Maximum laid-out lines. Zero allows any number of lines.</summary>
        public uint maxLines
        {
            get { return ManagedRuntimeContext.TextGetMaxLines(EntityId); }
            set { ManagedRuntimeContext.TextSetMaxLines(EntityId, value); }
        }

        public TextHorizontalAlignment horizontalAlignment
        {
            get { return (TextHorizontalAlignment)ManagedRuntimeContext.TextGetHorizontalAlignment(EntityId); }
            set { ManagedRuntimeContext.TextSetHorizontalAlignment(EntityId, (int)value); }
        }

        public TextVerticalAlignment verticalAlignment
        {
            get { return (TextVerticalAlignment)ManagedRuntimeContext.TextGetVerticalAlignment(EntityId); }
            set { ManagedRuntimeContext.TextSetVerticalAlignment(EntityId, (int)value); }
        }

        public FontStyle fontStyle
        {
            get { return (FontStyle)ManagedRuntimeContext.TextGetFontStyle(EntityId); }
            set { ManagedRuntimeContext.TextSetFontStyle(EntityId, (int)value); }
        }

        public Color outlineColor
        {
            get { return ManagedRuntimeContext.TextGetOutlineColor(EntityId); }
            set { ManagedRuntimeContext.TextSetOutlineColor(EntityId, value); }
        }

        public float outlineWidth
        {
            get { return ManagedRuntimeContext.TextGetOutlineWidth(EntityId); }
            set { ManagedRuntimeContext.TextSetOutlineWidth(EntityId, value); }
        }

        /// <summary>Color of the text shadow. A transparent color disables the shadow.</summary>
        public Color shadowColor
        {
            get { return ManagedRuntimeContext.TextGetShadowColor(EntityId); }
            set { ManagedRuntimeContext.TextSetShadowColor(EntityId, value); }
        }

        /// <summary>Shadow displacement from the glyph origin.</summary>
        public Vector2 shadowOffset
        {
            get { return ManagedRuntimeContext.TextGetShadowOffset(EntityId); }
            set { ManagedRuntimeContext.TextSetShadowOffset(EntityId, value); }
        }

        /// <summary>Additional edge softness applied to the text shadow.</summary>
        public float shadowSoftness
        {
            get { return ManagedRuntimeContext.TextGetShadowSoftness(EntityId); }
            set { ManagedRuntimeContext.TextSetShadowSoftness(EntityId, value); }
        }

        [Obsolete("Use outlineWidth instead.")]
        public float thickness
        {
            get { return outlineWidth; }
            set { outlineWidth = value; }
        }

        public float characterSpacing
        {
            get { return ManagedRuntimeContext.TextGetCharacterSpacing(EntityId); }
            set { ManagedRuntimeContext.TextSetCharacterSpacing(EntityId, value); }
        }

        /// <summary>Alias for characterSpacing.</summary>
        public float letterSpacing
        {
            get { return characterSpacing; }
            set { characterSpacing = value; }
        }

        public float wordSpacing
        {
            get { return ManagedRuntimeContext.TextGetWordSpacing(EntityId); }
            set { ManagedRuntimeContext.TextSetWordSpacing(EntityId, value); }
        }

        public float lineSpacing
        {
            get { return ManagedRuntimeContext.TextGetLineSpacing(EntityId); }
            set { ManagedRuntimeContext.TextSetLineSpacing(EntityId, value); }
        }

        /// <summary>Extra distance after an explicit newline. Soft wraps use lineSpacing.</summary>
        public float paragraphSpacing
        {
            get { return ManagedRuntimeContext.TextGetParagraphSpacing(EntityId); }
            set { ManagedRuntimeContext.TextSetParagraphSpacing(EntityId, value); }
        }

        /// <summary>Number of space advances used by a tab. Values below one are clamped.</summary>
        public uint tabWidth
        {
            get { return ManagedRuntimeContext.TextGetTabWidth(EntityId); }
            set { ManagedRuntimeContext.TextSetTabWidth(EntityId, value); }
        }

        /// <summary>Uses decorationColor instead of the text color for underline and strikethrough.</summary>
        public bool useCustomDecorationColor
        {
            get { return ManagedRuntimeContext.TextGetUseCustomDecorationColor(EntityId); }
            set { ManagedRuntimeContext.TextSetUseCustomDecorationColor(EntityId, value); }
        }

        /// <summary>Underline and strikethrough color when useCustomDecorationColor is enabled.</summary>
        public Color decorationColor
        {
            get { return ManagedRuntimeContext.TextGetDecorationColor(EntityId); }
            set { ManagedRuntimeContext.TextSetDecorationColor(EntityId, value); }
        }

        /// <summary>Decoration line width. Zero uses the font metric.</summary>
        public float decorationThickness
        {
            get { return ManagedRuntimeContext.TextGetDecorationThickness(EntityId); }
            set { ManagedRuntimeContext.TextSetDecorationThickness(EntityId, value); }
        }

        /// <summary>Offset added to the font-derived underline position.</summary>
        public float underlineOffset
        {
            get { return ManagedRuntimeContext.TextGetUnderlineOffset(EntityId); }
            set { ManagedRuntimeContext.TextSetUnderlineOffset(EntityId, value); }
        }

        /// <summary>Offset added to the font-derived strikethrough position.</summary>
        public float strikethroughOffset
        {
            get { return ManagedRuntimeContext.TextGetStrikethroughOffset(EntityId); }
            set { ManagedRuntimeContext.TextSetStrikethroughOffset(EntityId, value); }
        }

        public bool useKerning
        {
            get { return ManagedRuntimeContext.TextGetUseKerning(EntityId); }
            set { ManagedRuntimeContext.TextSetUseKerning(EntityId, value); }
        }

        /// <summary>Primary stable ordering key shared by text and sprites.</summary>
        public int SortingLayer
        {
            get { return ManagedRuntimeContext.TextGetSortingLayer(EntityId); }
            set { ManagedRuntimeContext.TextSetSortingLayer(EntityId, value); }
        }

        /// <summary>Ordering key within SortingLayer. Lower values render first.</summary>
        public int OrderInLayer
        {
            get { return ManagedRuntimeContext.TextGetOrderInLayer(EntityId); }
            set { ManagedRuntimeContext.TextSetOrderInLayer(EntityId, value); }
        }

        [Obsolete("Use SortingLayer instead.")]
        public int sortingLayer { get { return SortingLayer; } set { SortingLayer = value; } }

        [Obsolete("Use OrderInLayer instead.")]
        public int orderInLayer { get { return OrderInLayer; } set { OrderInLayer = value; } }

        /// <summary>
        /// Finds the nearest visible caret to a point in local text-layout coordinates.
        /// The returned value is a UTF-8 byte offset into <see cref="text"/>.
        /// </summary>
        public uint HitTest(Vector2 localPosition) => ManagedRuntimeContext.TextHitTest(EntityId, localPosition);

    }
}

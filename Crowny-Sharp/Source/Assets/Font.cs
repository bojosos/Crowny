using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Crowny
{
    /// <summary>Resolved geometry and atlas coordinates for one Unicode code point.</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct CharacterInfo
    {
        /// <summary>UUID of the primary or fallback font that supplied the glyph.</summary>
        public UUID sourceFont;

        /// <summary>Unicode code point requested by the caller.</summary>
        public uint requestedCodePoint;

        /// <summary>Unicode code point selected from the font, including replacement glyphs.</summary>
        public uint resolvedCodePoint;

        /// <summary>Index of the resolved glyph in its source font.</summary>
        public int glyphIndex;

        private uint reserved;

        /// <summary>Horizontal advance in font-em units.</summary>
        public double advance;

        /// <summary>Left edge of the glyph quad in font-em units.</summary>
        public double planeLeft;

        /// <summary>Bottom edge of the glyph quad in font-em units.</summary>
        public double planeBottom;

        /// <summary>Right edge of the glyph quad in font-em units.</summary>
        public double planeRight;

        /// <summary>Top edge of the glyph quad in font-em units.</summary>
        public double planeTop;

        /// <summary>Left edge of the glyph in atlas pixels.</summary>
        public double atlasLeft;

        /// <summary>Bottom edge of the glyph in atlas pixels.</summary>
        public double atlasBottom;

        /// <summary>Right edge of the glyph in atlas pixels.</summary>
        public double atlasRight;

        /// <summary>Top edge of the glyph in atlas pixels.</summary>
        public double atlasTop;

        /// <summary>Whether the glyph represents whitespace.</summary>
        [MarshalAs(UnmanagedType.I1)]
        public bool whitespace;

        /// <summary>Whether the font or one of its fallbacks resolved a glyph.</summary>
        [MarshalAs(UnmanagedType.I1)]
        public bool valid;
    }

    /// <summary>A static MSDF font asset imported by Crowny.</summary>
    public class Font : Asset
    {
        /// <summary>Whether the font has usable metrics, glyphs, and atlas data.</summary>
        public bool isValid => GetIsValid();

        /// <summary>Number of glyphs stored in the imported font.</summary>
        public uint glyphCount => GetGlyphCount();

        /// <summary>Number of space advances used for a tab.</summary>
        public uint tabWidth => GetTabWidth();

        /// <summary>Width of the imported atlas in pixels.</summary>
        public uint atlasWidth => GetAtlasWidth();

        /// <summary>Height of the imported atlas in pixels.</summary>
        public uint atlasHeight => GetAtlasHeight();

        /// <summary>Distance-field pixel range used to generate the atlas.</summary>
        public float atlasPixelRange => GetAtlasPixelRange();

        /// <summary>Number of runtime fallback fonts assigned to this font.</summary>
        public uint fallbackCount => GetFallbackCount();

        /// <summary>Checks whether this font contains an exact glyph for a Unicode code point.</summary>
        public bool HasGlyph(uint codePoint)
        {
            return HasGlyphNative(codePoint);
        }

        /// <summary>Checks whether this font contains an exact glyph for a UTF-16 character.</summary>
        public bool HasCharacter(char character)
        {
            return HasGlyphNative(character);
        }

        /// <summary>Resolves glyph data, optionally searching fallback fonts and replacement glyphs.</summary>
        public CharacterInfo GetCharacterInfo(uint codePoint, bool useFallbacks = true)
        {
            TryGetCharacterInfo(codePoint, out CharacterInfo characterInfo, useFallbacks);
            return characterInfo;
        }

        /// <summary>Attempts to resolve glyph data for a UTF-16 character.</summary>
        public bool GetCharacterInfo(char character, out CharacterInfo characterInfo, bool useFallbacks = true)
        {
            return TryGetCharacterInfo(character, out characterInfo, useFallbacks);
        }

        /// <summary>Attempts to resolve glyph data, optionally searching fallback fonts and replacement glyphs.</summary>
        public bool TryGetCharacterInfo(uint codePoint, out CharacterInfo characterInfo, bool useFallbacks = true)
        {
            return TryGetCharacterInfoNative(codePoint, useFallbacks, out characterInfo);
        }

        /// <summary>Returns a runtime fallback font by index, or null when the index is out of range.</summary>
        public Font GetFallback(uint index)
        {
            return GetFallbackNative(index);
        }

        /// <summary>Adds a runtime fallback font when it is usable, unique, and the chain has room.</summary>
        public bool AddFallback(Font font)
        {
            return font != null && AddFallbackNative(font);
        }

        /// <summary>Removes every runtime fallback font.</summary>
        public void ClearFallbacks()
        {
            ClearFallbacksNative();
        }

#if CROWNY_MONO
        private bool GetIsValid() => Internal_GetIsValid(m_InternalPtr);
        private uint GetGlyphCount() => Internal_GetGlyphCount(m_InternalPtr);
        private uint GetTabWidth() => Internal_GetTabWidth(m_InternalPtr);
        private uint GetAtlasWidth() => Internal_GetAtlasWidth(m_InternalPtr);
        private uint GetAtlasHeight() => Internal_GetAtlasHeight(m_InternalPtr);
        private float GetAtlasPixelRange() => Internal_GetAtlasPixelRange(m_InternalPtr);
        private bool HasGlyphNative(uint codePoint) => Internal_HasGlyph(m_InternalPtr, codePoint);
        private bool TryGetCharacterInfoNative(uint codePoint, bool useFallbacks, out CharacterInfo characterInfo) =>
            Internal_GetCharacterInfo(m_InternalPtr, codePoint, useFallbacks, out characterInfo);
        private uint GetFallbackCount() => Internal_GetFallbackCount(m_InternalPtr);
        private Font GetFallbackNative(uint index) => Internal_GetFallback(m_InternalPtr, index);
        private bool AddFallbackNative(Font font) => Internal_AddFallback(m_InternalPtr, font);
        private void ClearFallbacksNative() => Internal_ClearFallbacks(m_InternalPtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_GetIsValid(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern uint Internal_GetGlyphCount(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern uint Internal_GetTabWidth(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern uint Internal_GetAtlasWidth(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern uint Internal_GetAtlasHeight(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetAtlasPixelRange(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_HasGlyph(IntPtr thisptr, uint codePoint);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_GetCharacterInfo(IntPtr thisptr, uint codePoint, bool useFallbacks,
                                                              out CharacterInfo characterInfo);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern uint Internal_GetFallbackCount(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Font Internal_GetFallback(IntPtr thisptr, uint index);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_AddFallback(IntPtr thisptr, Font font);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_ClearFallbacks(IntPtr thisptr);
#else
        private bool GetIsValid() => ManagedRuntimeContext.FontGetIsValid(uuid);
        private uint GetGlyphCount() => ManagedRuntimeContext.FontGetGlyphCount(uuid);
        private uint GetTabWidth() => ManagedRuntimeContext.FontGetTabWidth(uuid);
        private uint GetAtlasWidth() => ManagedRuntimeContext.FontGetAtlasWidth(uuid);
        private uint GetAtlasHeight() => ManagedRuntimeContext.FontGetAtlasHeight(uuid);
        private float GetAtlasPixelRange() => ManagedRuntimeContext.FontGetAtlasPixelRange(uuid);
        private bool HasGlyphNative(uint codePoint) => ManagedRuntimeContext.FontHasGlyph(uuid, codePoint);
        private bool TryGetCharacterInfoNative(uint codePoint, bool useFallbacks, out CharacterInfo characterInfo)
        {
            characterInfo = ManagedRuntimeContext.FontGetCharacterInfo(uuid, codePoint, useFallbacks);
            return characterInfo.valid;
        }
        private uint GetFallbackCount() => ManagedRuntimeContext.FontGetFallbackCount(uuid);
        private Font GetFallbackNative(uint index) =>
            ManagedRuntimeContext.CreateAsset<Font>(ManagedRuntimeContext.FontGetFallback(uuid, index));
        private bool AddFallbackNative(Font font) =>
            ManagedRuntimeContext.FontAddFallback(uuid, font.uuid);
        private void ClearFallbacksNative() => ManagedRuntimeContext.FontClearFallbacks(uuid);
#endif
    }
}

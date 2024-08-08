using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Crowny
{
    [StructLayout(LayoutKind.Sequential)]
    public struct CharacterInfo
    {
        public int advance;
        public int glyphWidth;
        public int glyphHeight;
        public int bearing;
        public int minX;
        public int maxX;
        public int minY;
        public int maxY;
    }

    public class Font : Asset
    {
        public float fontSize
        {
            get { return Internal_GetFontSize(m_InternalPtr); }
            set { Internal_SetFontSize(m_InternalPtr, value); }
        }

        public bool dynamic
        {
            get { return Internal_GetIsDynamic(m_InternalPtr); }
            set { Internal_SetIsDynamic(m_InternalPtr, value); }
        }

        public CharacterInfo[] characterInfo
        {
            get { return Internal_GetCharacterInfos(m_InternalPtr); }
            set { Internal_SetCharacterInfos(m_InternalPtr, value); }
        }

        public bool GetCharacterInfo(char c, out CharacterInfo characterInfo, int size = 0, FontStyle style = FontStyle.None)
        {
            return Internal_GetCharacterInfo(m_InternalPtr, c, out characterInfo, size, style);
        }

        public bool HasCharacter(char c)
        {
            return Internal_HasCharacter(m_InternalPtr, c);
        }

        public void RequestCharacters(string characters, int size = 0, FontStyle style = FontStyle.None)
        {
            Internal_RequestCharacters(characters, size, style);
        }

        public static Font CreateDynamicFont()
        {
            return Internal_CreateDynamicFont();
        }

        public static string[] GetPathsToOSFonts()
        {
            return Internal_GetPathsToOSFonts();
        }

        public static string[] GetInstalledFontNames()
        {
            return Internal_GetInstalledFontNames();
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetFontSize(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetFontSize(IntPtr thisptr, float size);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_GetIsDynamic(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetIsDynamic(IntPtr thisptr, bool dynamic);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetCharacterInfos(IntPtr thisptr, CharacterInfo[] characterInfos);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern CharacterInfo[] Internal_GetCharacterInfos(IntPtr thisptr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_GetCharacterInfo(IntPtr thisPtr, char c, out CharacterInfo characterInfo, int size, FontStyle style);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_HasCharacter(IntPtr thisPtr, char c);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_RequestCharacters(string characters, int size, FontStyle style);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Font Internal_CreateDynamicFont();
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string[] Internal_GetInstalledFontNames();
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string[] Internal_GetPathsToOSFonts();
    }
}
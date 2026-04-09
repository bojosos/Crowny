using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    public class Texture : Asset
    {
        public uint width => Internal_GetWidth(m_InternalPtr);
        public uint height => Internal_GetHeight(m_InternalPtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern uint Internal_GetWidth(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern uint Internal_GetHeight(IntPtr thisPtr);
    }
}

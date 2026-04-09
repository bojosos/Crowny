using System;
using System.Runtime.CompilerServices;

namespace Crowny
{

    public class Mesh : Asset
    {
        public uint vertexCount => Internal_GetVertexCount(m_InternalPtr);
        public uint indexCount => Internal_GetIndexCount(m_InternalPtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern uint Internal_GetVertexCount(IntPtr thisPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern uint Internal_GetIndexCount(IntPtr thisPtr);
    }
}

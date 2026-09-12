using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
    public class ScriptObject
    {
#if CROWNY_MONO
        internal IntPtr m_InternalPtr;

        ~ScriptObject()
        {
            if (m_InternalPtr != IntPtr.Zero)
                Internal_ManagedInstanceDeleted(m_InternalPtr);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_ManagedInstanceDeleted(IntPtr instance);
#endif
    }
}

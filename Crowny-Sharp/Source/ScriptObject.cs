using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
	public class ScriptObject
	{
		internal IntPtr m_InternalPtr;

#if CROWNY_MONO
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

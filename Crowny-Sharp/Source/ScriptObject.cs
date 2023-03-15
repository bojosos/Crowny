using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
	public class ScriptObject
	{
		internal IntPtr m_InternalPtr;

		~ScriptObject()
		{
			if (m_InternalPtr == IntPtr.Zero)
				Debug.Error("Wat");
			Internal_ManagedInstanceDeleted(m_InternalPtr);
		}

		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern void Internal_ManagedInstanceDeleted(IntPtr instance);
	}
}
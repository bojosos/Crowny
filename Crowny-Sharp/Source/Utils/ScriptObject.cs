using System;

namespace Crowny
{
	public class ScriptObject
	{
		internal IntPtr m_InternalPtr;

		~ScriptObject()
		{
			// TODO: Notify native that I have been destroyed
		}
	}
}
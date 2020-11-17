using System;

namespace Crowny
{
	public class ScriptObject
	{
		internal IntPtr m_InternalPtr;

		~ScriptObject()
		{
			// TODO: Notift native that I have been destroyed
		}
	}
}
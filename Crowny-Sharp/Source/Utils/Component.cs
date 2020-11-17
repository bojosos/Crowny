using System;
using System.Runtime.CompilerServices;

namespace Crowny
{

	public class Component : ScriptObject
	{

		public Entity entity { get { Console.WriteLine(m_InternalPtr); Console.WriteLine("Getting entity"); Entity e = Internal_GetEntity(m_InternalPtr); Console.WriteLine("Entity"); return e;} }

		public Transform transform { get { Console.WriteLine("Getting transform"); return entity.transform; } }

		[MethodImpl(MethodImplOptions.InternalCall)]
		public static extern Component Internal_GetComponent(Entity parent, Type type);

		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern Entity Internal_GetEntity(IntPtr internalPtr);

		// Might remove 
		[MethodImpl(MethodImplOptions.InternalCall)]
		extern void Internal_GetComponent(Type type, IntPtr oneAfterResult);
	}

}
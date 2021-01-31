using System;
using System.Runtime.CompilerServices;

namespace Crowny
{

	public class Component : ScriptObject
	{

		public Entity entity { get { return Internal_GetEntity(m_InternalPtr); } }

		public Transform transform { get { return entity.transform; } }

		public T GetComponent<T>() where T : Component
		{
			return (T)Internal_GetComponent(m_InternalPtr, typeof(T));
		}

		public bool HasComponent<T>() where T : Component
		{
			return Internal_HasComponent(m_InternalPtr, typeof(T));
		}

		public T AddComponent<T>(T t) where T : Component
		{
			return (T)Internal_AddComponent(m_InternalPtr, typeof(T));
		}

		[MethodImpl(MethodImplOptions.InternalCall)]
		public static extern Component Internal_GetComponent(IntPtr parent, Type type);
		[MethodImpl(MethodImplOptions.InternalCall)]
		public static extern bool Internal_HasComponent(IntPtr parent, Type type);
		[MethodImpl(MethodImplOptions.InternalCall)]
		public static extern Component Internal_AddComponent(IntPtr parent, Type type);
		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern Entity Internal_GetEntity(IntPtr internalPtr);
	}

}
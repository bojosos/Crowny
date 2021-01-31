using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
	public class Entity : GameObject
	{
		public string Name
		{
			get { return Internal_GetName(m_InternalPtr); }
			set { Internal_SetName(m_InternalPtr, value); }
		}

		public Entity Parent
		{
			get { return Internal_GetParent(m_InternalPtr); }
			//set { Internal_SetParent(m_InternalPtr, value); }
		}

		public Transform transform
		{
			get { return Internal_GetTransform(m_InternalPtr); }
		}

		//public T GetComponent<T>() where T : Component
		//{
			//return (T)Component.Internal_GetComponent(this, typeof(T));
		//}

		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern string Internal_GetName(IntPtr internalPtr);
		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern void Internal_SetName(IntPtr internalPtr, string value);
		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern Entity Internal_GetParent(IntPtr internalPtr);
		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern Transform Internal_GetTransform(IntPtr internalPtr);
	}

}

using System;
using System.Runtime.CompilerServices;

namespace Crowny
{

	public class Component : ScriptObject
	{

		/// <summary>
		/// Returns the game object of this component.
		/// </summary>
		/// <returns>The game object.</returns>
		public Entity entity { get { return Internal_GetEntity(m_InternalPtr); } }

		public Transform transform { get { return entity.transform; } }

		/// <summary>
		/// Retrieves a component.
		/// </summary>
		/// <typeparam name="T">Type of the compoenent.</typeparam>
		/// <returns>Returns the component if the operation was successful, otherwise nullptr.</returns>
		public T GetComponent<T>() where T : Component
		{
			return (T)Internal_GetComponent(m_InternalPtr, typeof(T));
		}

		/// <summary>
		/// Determines if a game object has a component.
		/// </summary>
		/// <typeparam name="T">The type of the component.</typeparam>
		/// <returns>Whether the game object has the component.</returns>
		public bool HasComponent<T>() where T : Component
		{
			return Internal_HasComponent(m_InternalPtr, typeof(T));
		}

		/// <summary>
		/// Adds a new component to the game object.
		/// </summary>
		/// <param name="t">Type of the component.</param>
		/// <typeparam name="T"></typeparam>
		/// <returns>Returns the component if the operation was successful, otherwise nullptr.</returns>
		public T AddComponent<T>(T t) where T : Component
		{
			return (T)Internal_AddComponent(m_InternalPtr, typeof(T));
		}

		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern Component Internal_GetComponent(IntPtr parent, Type type);
		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern bool Internal_HasComponent(IntPtr parent, Type type);
		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern Component Internal_AddComponent(IntPtr parent, Type type);
		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern Entity Internal_GetEntity(IntPtr internalPtr);
	}

}
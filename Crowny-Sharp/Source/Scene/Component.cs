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

		/// <value>The transform of the object</value>
		public Transform transform { get { return GetComponent<Transform>(); } }

		/// <summary>
		/// Retrieves a component.
		/// </summary>
		/// <typeparam name="T">Type of the compoenent.</typeparam>
		/// <returns>Returns the component if the operation was successful, otherwise nullptr.</returns>
		public T GetComponent<T>() where T : Component
		{
			return (T)Internal_GetComponent(entity, typeof(T));
		}

		/// <summary>
		/// Determines if a game object has a component.
		/// </summary>
		/// <typeparam name="T">The type of the component.</typeparam>
		/// <returns>Whether the game object has the component.</returns>
		public bool HasComponent<T>() where T : Component
		{
			return Internal_HasComponent(entity, typeof(T));
		}

		/// <summary>
		/// Adds a new component to the game object.
		/// </summary>
		/// <param name="t">Type of the component.</param>
		/// <typeparam name="T"></typeparam>
		/// <returns>Returns the component if the operation was successful, otherwise nullptr.</returns>
		public T AddComponent<T>(T t) where T : Component
		{
			return (T)Internal_AddComponent(entity, typeof(T));
		}

		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern Component Internal_GetComponent(Entity parent, Type type);
		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern bool Internal_HasComponent(Entity parent, Type type);
		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern Component Internal_AddComponent(Entity parent, Type type);
		[MethodImpl(MethodImplOptions.InternalCall)]
		private static extern Entity Internal_GetEntity(IntPtr internalPtr);
	}

}
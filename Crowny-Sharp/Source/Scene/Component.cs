using System;
using System.Runtime.CompilerServices;

namespace Crowny
{

	public class Component : ScriptObject
	{

		/// <summary>
		/// Returns the parent entity of this component.
		/// </summary>
		/// <returns>The entity.</returns>
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
			return entity.GetComponent<T>();
		}

		/// <summary>
		/// Determines if a entity has a component.
		/// </summary>
		/// <typeparam name="T">The type of the component.</typeparam>
		/// <returns>Whether the game object has the component.</returns>
		public bool HasComponent<T>() where T : Component
		{
			return entity.HasComponent<T>();
		}

		/// <summary>
		/// Adds a new component to the entity.
		/// </summary>
		/// <returns>Returns the component if the operation was successful, otherwise nullptr.</returns>
		public T AddComponent<T>() where T : Component
		{
			return entity.AddComponent<T>();
		}

        /// <summary>
        /// Removes the component from the entity.
        /// </summary>
        /// <returns>The component.</returns>
        /// <param name="t">T.</param>
        /// <typeparam name="T">The 1st type parameter.</typeparam>
        public void RemoveComponent<T>() where T : Component
        {
            entity.RemoveComponent<T>();
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Entity Internal_GetEntity(IntPtr internalPtr);
	}

}
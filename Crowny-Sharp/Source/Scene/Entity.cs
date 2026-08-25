using System;
using System.Runtime.CompilerServices;

namespace Crowny
{
	public class Entity : ScriptObject
	{
		/// <summary>
		/// The name of the entity.
		/// </summary>
		/// <value>The entity name.</value>
		public string name
		{
#if CROWNY_MONO
			get { return Internal_GetName(m_InternalPtr); }
			set { Internal_SetName(m_InternalPtr, value); }
#else
            get { return ManagedRuntimeContext.GetEntityName(m_ManagedUuid); }
            set { ManagedRuntimeContext.SetEntityName(m_ManagedUuid, value); }
#endif
		}

        /// <summary>
        /// The UUID of the entity.
        /// </summary>
        public UUID uuid
        {
            get
            {
#if CROWNY_MONO
                Internal_GetUUID(m_InternalPtr, out UUID value);
                return value;
#else
                return m_ManagedUuid;
#endif
            }
        }

#if !CROWNY_MONO
        internal UUID m_ManagedUuid;
#endif

        /// <summary>
        /// The parent of the entity.
        /// </summary>
        /// <value>The parent entity.</value>
		public Entity parent
		{
#if CROWNY_MONO
			get { return Internal_GetParent(m_InternalPtr); }
			set { Internal_SetParent(m_InternalPtr, value); }
#else
            get
            {
                UUID parentUuid = ManagedRuntimeContext.GetEntityParent(m_ManagedUuid);
                return parentUuid == UUID.Empty ? null : new Entity { m_ManagedUuid = parentUuid };
            }
            set
            {
                ManagedRuntimeContext.SetEntityParent(m_ManagedUuid, value != null ? value.m_ManagedUuid : UUID.Empty);
            }
#endif
		}

		/// <summary>
		/// The transform component of the entity.
		/// </summary>
		/// <value>Transform component.</value>
		public Transform transform
		{
			get { return GetComponent<Transform>(); }
		}

		/// <summary>
        /// Searches for an entity by its name.
        /// </summary>
        /// <param name="name">The name of the game object.</param>
        /// <returns>The object if found, null otherwise.</returns>
        public static Entity FindByName(string name)
        {
#if CROWNY_MONO
           return Internal_FindByName(name);
#else
           UUID entityUuid = ManagedRuntimeContext.FindEntityByName(name);
           return entityUuid == UUID.Empty ? null : new Entity { m_ManagedUuid = entityUuid };
#endif
        }

        /// <summary>
        /// Retrieves a component. It can also be used to retrieve script components.
        /// For example lets say you have an entity behavior called "PlayerController".
        /// You can do GetComponent<PlayerController>() on its entity and it will just work.
        /// </summary>
        /// <typeparam name="T">Type of the compoenent.</typeparam>
        /// <returns>Returns the component if the operation was successful, otherwise null.</returns>
        public T GetComponent<T>() where T : Component
        {
            return (T)Internal_GetComponent(m_InternalPtr, typeof(T));
        }

        /// <summary>
        /// Determines if a entity has a component.
        /// </summary>
        /// <typeparam name="T">The type of the component.</typeparam>
        /// <returns>Whether the entity has the component.</returns>
        public bool HasComponent<T>() where T : Component
        {
            return Internal_HasComponent(m_InternalPtr, typeof(T));
        }

        /// <summary>
        /// Adds a new component to the entity.
        /// </summary>
        /// <returns>Returns the component if the operation was successful, otherwise null.</returns>
        public T AddComponent<T>() where T : Component
        {
            return (T)Internal_AddComponent(m_InternalPtr, typeof(T));
        }

        /// <summary>
        /// Removes the component from the entity.
        /// </summary>
        /// <typeparam name="T">The type of component.</typeparam>
        public void RemoveComponent<T>() where T : Component
        {
            Internal_RemoveComponent(m_InternalPtr, typeof(T));
        }

        public void Destroy()
        {
#if CROWNY_MONO
            Internal_Destroy(m_InternalPtr);
#else
            ManagedRuntimeContext.DestroyEntity(m_ManagedUuid);
#endif
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Component Internal_GetComponent(IntPtr parent, Type type);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_HasComponent(IntPtr parent, Type type);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Component Internal_AddComponent(IntPtr parent, Type type);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_RemoveComponent(IntPtr parent, Type type);

#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_GetUUID(IntPtr parent, out UUID uuid);
#endif
#if CROWNY_MONO
        [MethodImpl(MethodImplOptions.InternalCall)] private static extern string Internal_GetName(IntPtr internalPtr);
		[MethodImpl(MethodImplOptions.InternalCall)] private static extern void Internal_SetName(IntPtr internalPtr, string value);
		[MethodImpl(MethodImplOptions.InternalCall)] private static extern Entity Internal_GetParent(IntPtr internalPtr);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Entity Internal_SetParent(IntPtr internalPtr, Entity entity);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Entity Internal_FindByName(string name);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_Destroy(IntPtr internalPtr);
#endif
	}

}

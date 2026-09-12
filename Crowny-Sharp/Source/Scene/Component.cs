using System;

namespace Crowny
{

    public class Component : ScriptObject
    {
        /// <summary>
        /// Returns the parent entity of this component.
        /// </summary>
        /// <returns>The entity.</returns>
        [DontSerializeField]
        public Entity entity
        {
            get
            {
                if (m_ManagedEntityId == UUID.Empty)
                    throw new InvalidOperationException("The component is not attached to an entity.");
                return ManagedRuntimeContext.GetComponentEntity(m_ManagedEntityId, ref m_ComponentCache);
            }
        }

        internal UUID m_ManagedEntityId;
        internal ManagedRuntimeContext.ComponentCache m_ComponentCache;

        protected UUID EntityId
        {
            get
            {
                if (m_ManagedEntityId == UUID.Empty)
                    throw new InvalidOperationException("The component is not attached to an entity.");
                return m_ManagedEntityId;
            }
        }

        /// <value>The transform of the object</value>
        [DontSerializeField]
        public Transform transform { get { return ManagedRuntimeContext.GetTransform(EntityId, ref m_ComponentCache); } }

        /// <summary>
        /// Retrieves a component.
        /// </summary>
        /// <typeparam name="T">Type of the compoenent.</typeparam>
        /// <returns>Returns the component if the operation was successful, otherwise nullptr.</returns>
        public T GetComponent<T>() where T : Component
        {
            return ManagedRuntimeContext.GetComponent<T>(EntityId, ref m_ComponentCache);
        }

        /// <summary>
        /// Determines if a entity has a component.
        /// </summary>
        /// <typeparam name="T">The type of the component.</typeparam>
        /// <returns>Whether the game object has the component.</returns>
        public bool HasComponent<T>() where T : Component
        {
            return ManagedRuntimeContext.HasComponent<T>(EntityId);
        }

        /// <summary>
        /// Adds a new component to the entity.
        /// </summary>
        /// <returns>Returns the component if the operation was successful, otherwise nullptr.</returns>
        public T AddComponent<T>() where T : Component
        {
            return ManagedRuntimeContext.AddComponent<T>(EntityId, ref m_ComponentCache);
        }

        /// <summary>
        /// Removes the component from the entity.
        /// </summary>
        /// <returns>The component.</returns>
        /// <param name="t">T.</param>
        /// <typeparam name="T">The 1st type parameter.</typeparam>
        public void RemoveComponent<T>() where T : Component
        {
            ManagedRuntimeContext.RemoveComponent<T>(EntityId);
        }

    }

}

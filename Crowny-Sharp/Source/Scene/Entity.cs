namespace Crowny
{
    public class Entity : ScriptObject
    {
        internal UUID m_ManagedUuid;

        /// <summary>The name of the entity.</summary>
        public string name
        {
            get { return ManagedRuntimeContext.GetEntityName(uuid); }
            set { ManagedRuntimeContext.SetEntityName(uuid, value); }
        }

        /// <summary>The UUID of the entity.</summary>
        public UUID uuid => m_ManagedUuid;

        /// <summary>The parent of the entity.</summary>
        public Entity parent
        {
            get
            {
                UUID parentUuid = ManagedRuntimeContext.GetEntityParent(m_ManagedUuid);
                return parentUuid == UUID.Empty ? null : new Entity { m_ManagedUuid = parentUuid };
            }
            set { ManagedRuntimeContext.SetEntityParent(m_ManagedUuid, value?.m_ManagedUuid ?? UUID.Empty); }
        }

        /// <summary>The transform component of the entity.</summary>
        public Transform transform => GetComponent<Transform>();

        /// <summary>Searches for an entity by its name.</summary>
        public static Entity FindByName(string name)
        {
            UUID entityUuid = ManagedRuntimeContext.FindEntityByName(name);
            return entityUuid == UUID.Empty ? null : new Entity { m_ManagedUuid = entityUuid };
        }

        /// <summary>Retrieves a native or managed script component.</summary>
        public T GetComponent<T>() where T : Component
        {
            return ManagedRuntimeContext.GetComponent<T>(m_ManagedUuid);
        }

        /// <summary>Returns whether the entity has a component.</summary>
        public bool HasComponent<T>() where T : Component
        {
            return ManagedRuntimeContext.HasComponent<T>(m_ManagedUuid);
        }

        /// <summary>Adds a component to the entity.</summary>
        public T AddComponent<T>() where T : Component
        {
            return ManagedRuntimeContext.AddComponent<T>(m_ManagedUuid);
        }

        /// <summary>Removes a component from the entity.</summary>
        public void RemoveComponent<T>() where T : Component
        {
            ManagedRuntimeContext.RemoveComponent<T>(m_ManagedUuid);
        }

        /// <summary>Destroys the entity.</summary>
        public void Destroy()
        {
            ManagedRuntimeContext.DestroyEntity(uuid);
        }
    }
}

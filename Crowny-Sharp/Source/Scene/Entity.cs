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

        /// <summary>Instantiates a prefab into the active scene at its saved transform.</summary>
        public static Entity Instantiate(Prefab prefab)
        {
            return Instantiate(prefab, (Entity)null);
        }

        /// <summary>Instantiates a prefab into the active scene, parented to parent while keeping the prefab's world transform.</summary>
        public static Entity Instantiate(Prefab prefab, Entity parent)
        {
            if (prefab == null)
                throw new System.ArgumentNullException("prefab");
            UUID entityUuid = ManagedRuntimeContext.EntityInstantiatePrefab(prefab.uuid, parent?.m_ManagedUuid ?? UUID.Empty);
            return entityUuid == UUID.Empty ? null : new Entity { m_ManagedUuid = entityUuid };
        }

        /// <summary>Instantiates a prefab into the active scene at the given world position.</summary>
        public static Entity Instantiate(Prefab prefab, Vector3 position)
        {
            return Instantiate(prefab, position, Quaternion.identity, null);
        }

        /// <summary>Instantiates a prefab into the active scene at the given world position and rotation.</summary>
        public static Entity Instantiate(Prefab prefab, Vector3 position, Quaternion rotation)
        {
            return Instantiate(prefab, position, rotation, null);
        }

        /// <summary>Instantiates a prefab into the active scene at the given world pose, parented to parent.</summary>
        public static Entity Instantiate(Prefab prefab, Vector3 position, Quaternion rotation, Entity parent)
        {
            Entity instance = Instantiate(prefab, parent);
            if (instance == null)
                return null;
            instance.transform.position = position;
            instance.transform.rotation = rotation;
            return instance;
        }

        /// <summary>Instantiates a copy of an entity and its entire hierarchy with fresh identities.</summary>
        public static Entity Instantiate(Entity source)
        {
            return Instantiate(source, (Entity)null);
        }

        /// <summary>Instantiates a copy of an entity and its entire hierarchy, parented to parent.</summary>
        public static Entity Instantiate(Entity source, Entity parent)
        {
            if (source == null)
                throw new System.ArgumentNullException("source");
            UUID entityUuid = ManagedRuntimeContext.EntityInstantiateEntity(source.uuid, parent?.m_ManagedUuid ?? UUID.Empty);
            return entityUuid == UUID.Empty ? null : new Entity { m_ManagedUuid = entityUuid };
        }

        /// <summary>Instantiates a copy of an entity and its entire hierarchy at the given world pose.</summary>
        public static Entity Instantiate(Entity source, Vector3 position, Quaternion rotation)
        {
            Entity instance = Instantiate(source, (Entity)null);
            if (instance == null)
                return null;
            instance.transform.position = position;
            instance.transform.rotation = rotation;
            return instance;
        }
    }
}

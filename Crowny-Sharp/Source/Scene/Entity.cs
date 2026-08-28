namespace Crowny
{
    public partial class Entity : ScriptObject
    {
        /// <summary>The name of the entity.</summary>
        public string name
        {
            get { return ManagedRuntimeContext.GetEntityName(uuid); }
            set { ManagedRuntimeContext.SetEntityName(uuid, value); }
        }

        /// <summary>The UUID of the entity.</summary>
        public UUID uuid => GetUuid();

        /// <summary>The parent of the entity.</summary>
        public Entity parent
        {
            get { return GetParent(); }
            set { SetParent(value); }
        }

        /// <summary>The transform component of the entity.</summary>
        public Transform transform => GetComponent<Transform>();

        /// <summary>Searches for an entity by its name.</summary>
        public static Entity FindByName(string name)
        {
            return FindByNameBinding(name);
        }

        /// <summary>Retrieves a native or managed script component.</summary>
        public T GetComponent<T>() where T : Component
        {
            return GetComponentBinding<T>();
        }

        /// <summary>Returns whether the entity has a component.</summary>
        public bool HasComponent<T>() where T : Component
        {
            return HasComponentBinding<T>();
        }

        /// <summary>Adds a component to the entity.</summary>
        public T AddComponent<T>() where T : Component
        {
            return AddComponentBinding<T>();
        }

        /// <summary>Removes a component from the entity.</summary>
        public void RemoveComponent<T>() where T : Component
        {
            RemoveComponentBinding<T>();
        }

        /// <summary>Destroys the entity.</summary>
        public void Destroy()
        {
            ManagedRuntimeContext.DestroyEntity(uuid);
        }
    }
}

#if !CROWNY_MONO
namespace Crowny
{
    public partial class Entity
    {
        internal UUID m_ManagedUuid;

        private UUID GetUuid() => m_ManagedUuid;

        private Entity GetParent()
        {
            UUID parentUuid = ManagedRuntimeContext.GetEntityParent(m_ManagedUuid);
            return parentUuid == UUID.Empty ? null : new Entity { m_ManagedUuid = parentUuid };
        }

        private void SetParent(Entity value)
        {
            ManagedRuntimeContext.SetEntityParent(m_ManagedUuid, value != null ? value.m_ManagedUuid : UUID.Empty);
        }

        private static Entity FindByNameBinding(string name)
        {
            UUID entityUuid = ManagedRuntimeContext.FindEntityByName(name);
            return entityUuid == UUID.Empty ? null : new Entity { m_ManagedUuid = entityUuid };
        }

        private T GetComponentBinding<T>() where T : Component => ManagedRuntimeContext.GetComponent<T>(m_ManagedUuid);
        private bool HasComponentBinding<T>() where T : Component => ManagedRuntimeContext.HasComponent<T>(m_ManagedUuid);
        private T AddComponentBinding<T>() where T : Component => ManagedRuntimeContext.AddComponent<T>(m_ManagedUuid);
        private void RemoveComponentBinding<T>() where T : Component => ManagedRuntimeContext.RemoveComponent<T>(m_ManagedUuid);
    }
}
#endif

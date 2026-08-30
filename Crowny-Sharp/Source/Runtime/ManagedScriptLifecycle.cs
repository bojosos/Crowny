using System;

namespace Crowny
{
    internal static class ManagedScriptLifecycle
    {
        internal static string TryPrepare(object instance)
        {
            try
            {
                Component component = instance as Component;
                if (component == null || component.m_ManagedEntityId == UUID.Empty)
                    throw new InvalidOperationException("A managed script must be attached to a live entity before initialization.");

                object[] requirements = instance.GetType().GetCustomAttributes(typeof(RequireComponent), true);
                foreach (RequireComponent requirement in requirements)
                {
                    foreach (Type requiredType in requirement.Components)
                    {
                        if (requiredType.IsAssignableFrom(instance.GetType()))
                            continue;
                        EnsureComponent(component.m_ManagedEntityId, requiredType);
                    }
                }
                return null;
            }
            catch (Exception error)
            {
                return error.ToString();
            }
        }

        private static void EnsureComponent(UUID entity, Type type)
        {
            if (!typeof(Component).IsAssignableFrom(type))
                throw new ArgumentException("Required managed types must derive from Component.", "type");
            if (typeof(EntityBehaviour).IsAssignableFrom(type))
            {
                if (ManagedRuntimeAdapter.ResolveScriptComponent(entity, type) == null)
                    ManagedRuntimeContext.AddScriptComponent(entity, type);
                return;
            }
            string typeName = type.FullName ?? type.Name;
            if (!ManagedRuntimeContext.EntityHasComponent(entity, typeName))
                ManagedRuntimeContext.EntityAddComponent(entity, typeName);
        }
    }
}

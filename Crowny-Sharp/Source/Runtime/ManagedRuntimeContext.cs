using System;

namespace Crowny
{
    internal static class ManagedRuntimeContext
    {
        [ThreadStatic]
        private static float callbackDeltaTime;
        private static Action<int, string> logHandler;
        private static Func<UUID, string> getEntityName;
        private static Action<UUID, string> setEntityName;
        private static Func<string, UUID> findEntityByName;
        private static Func<UUID, UUID> getEntityParent;
        private static Action<UUID, UUID> setEntityParent;
        private static Action<UUID> destroyEntity;

        internal static float DeltaTime => callbackDeltaTime;

        internal static CallbackScope Push(float deltaTime)
        {
            float previous = callbackDeltaTime;
            callbackDeltaTime = deltaTime;
            return new CallbackScope(previous);
        }

        internal static void SetLogHandler(Action<int, string> handler)
        {
            logHandler = handler;
        }

        internal static void WriteLog(int severity, string message)
        {
            logHandler?.Invoke(severity, message);
        }

        internal static void SetEntityHandlers(Func<UUID, string> getName, Action<UUID, string> setName, Func<string, UUID> findByName,
                                               Func<UUID, UUID> getParent, Action<UUID, UUID> setParent, Action<UUID> destroy)
        {
            getEntityName = getName;
            setEntityName = setName;
            findEntityByName = findByName;
            getEntityParent = getParent;
            setEntityParent = setParent;
            destroyEntity = destroy;
        }

        internal static string GetEntityName(UUID entity) =>
            getEntityName != null ? getEntityName(entity) : throw new InvalidOperationException("Managed entity bindings are unavailable.");

        internal static void SetEntityName(UUID entity, string name)
        {
            if (setEntityName == null)
                throw new InvalidOperationException("Managed entity bindings are unavailable.");
            setEntityName(entity, name);
        }

        internal static UUID FindEntityByName(string name) =>
            findEntityByName != null ? findEntityByName(name) : throw new InvalidOperationException("Managed entity bindings are unavailable.");

        internal static UUID GetEntityParent(UUID entity) =>
            getEntityParent != null ? getEntityParent(entity) : throw new InvalidOperationException("Managed entity bindings are unavailable.");

        internal static void SetEntityParent(UUID entity, UUID parent)
        {
            if (setEntityParent == null)
                throw new InvalidOperationException("Managed entity bindings are unavailable.");
            setEntityParent(entity, parent);
        }

        internal static void DestroyEntity(UUID entity)
        {
            if (destroyEntity == null)
                throw new InvalidOperationException("Managed entity bindings are unavailable.");
            destroyEntity(entity);
        }

        internal struct CallbackScope : IDisposable
        {
            private readonly float previous;

            internal CallbackScope(float previousDeltaTime)
            {
                previous = previousDeltaTime;
            }

            public void Dispose()
            {
                callbackDeltaTime = previous;
            }
        }
    }
}

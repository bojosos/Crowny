using System;

namespace Crowny
{
    public enum SceneExecutionState : byte
    {
        Edit,
        Play,
        PlayPaused,
        Simulate
    }

    public enum SceneOperationStatus : byte
    {
        Completed,
        Deferred,
        InvalidScene,
        Failed
    }

    internal enum SceneLifecycleEventType : byte
    {
        Loaded,
        Unloaded,
        Reloaded,
        ActiveChanged,
        ExecutionStateChanged
    }

    /// <summary>Stable asset reference for a scene. It does not expose a project-relative or absolute path.</summary>
    [SerializeObject]
    public struct SceneReference : IEquatable<SceneReference>
    {
        public UUID uuid;

        public SceneReference(UUID uuid)
        {
            this.uuid = uuid;
        }

        public bool IsValid { get { return uuid != UUID.Empty; } }
        public bool Equals(SceneReference other) { return uuid == other.uuid; }
        public override bool Equals(object obj) { return obj is SceneReference && Equals((SceneReference)obj); }
        public override int GetHashCode() { return uuid.GetHashCode(); }
        public static bool operator ==(SceneReference left, SceneReference right) { return left.Equals(right); }
        public static bool operator !=(SceneReference left, SceneReference right) { return !left.Equals(right); }
    }

    /// <summary>Loads and selects scenes by asset UUID. Changes requested from callbacks are applied after callback dispatch.</summary>
    public static class SceneManager
    {
        public static event Action<UUID> sceneLoaded;
        public static event Action<UUID> sceneUnloaded;
        public static event Action<UUID> sceneReloaded;
        public static event Action<UUID> activeSceneChanged;
        public static event Action<SceneExecutionState> executionStateChanged;

        public static UUID activeScene
        {
            get { return ManagedRuntimeContext.SceneGetActive(); }
        }

        public static SceneExecutionState executionState { get { return (SceneExecutionState)ManagedRuntimeContext.SceneGetExecutionState(); } }

        public static UUID[] loadedScenes
        {
            get
            {
                uint count = ManagedRuntimeContext.SceneGetLoadedCount();
                UUID[] scenes = new UUID[(int)count];
                for (uint i = 0; i < count; ++i)
                    scenes[i] = ManagedRuntimeContext.SceneGetLoaded(i);
                return scenes;
            }
        }

        public static SceneOperationStatus Load(UUID scene, bool makeActive = true) { return (SceneOperationStatus)ManagedRuntimeContext.SceneLoad(scene, makeActive); }
        public static SceneOperationStatus Load(SceneReference scene, bool makeActive = true) { return Load(scene.uuid, makeActive); }
        public static SceneOperationStatus Unload(UUID scene) { return (SceneOperationStatus)ManagedRuntimeContext.SceneUnload(scene); }
        public static SceneOperationStatus Unload(SceneReference scene) { return Unload(scene.uuid); }
        public static SceneOperationStatus Reload(UUID scene) { return (SceneOperationStatus)ManagedRuntimeContext.SceneReload(scene); }
        public static SceneOperationStatus Reload(SceneReference scene) { return Reload(scene.uuid); }
        public static SceneOperationStatus SetActive(UUID scene) { return (SceneOperationStatus)ManagedRuntimeContext.SceneSetActive(scene); }
        public static SceneOperationStatus SetActive(SceneReference scene) { return SetActive(scene.uuid); }

        internal static void NotifySceneEvent(SceneLifecycleEventType type, UUID scene, SceneExecutionState state)
        {
            switch (type)
            {
                case SceneLifecycleEventType.Loaded:
                    sceneLoaded?.Invoke(scene);
                    break;
                case SceneLifecycleEventType.Unloaded:
                    sceneUnloaded?.Invoke(scene);
                    break;
                case SceneLifecycleEventType.Reloaded:
                    sceneReloaded?.Invoke(scene);
                    break;
                case SceneLifecycleEventType.ActiveChanged:
                    activeSceneChanged?.Invoke(scene);
                    break;
                case SceneLifecycleEventType.ExecutionStateChanged:
                    executionStateChanged?.Invoke(state);
                    break;
            }
        }

    }
}

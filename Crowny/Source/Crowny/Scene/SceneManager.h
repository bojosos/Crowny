#pragma once

#include "Crowny/Common/Module.h"
#include "Crowny/Scene/Scene.h"

namespace Crowny
{
    enum class SceneExecutionState : uint8_t
    {
        Edit,
        Play,
        PlayPaused,
        Simulate
    };

    enum class SceneOperationStatus : uint8_t
    {
        Completed,
        Deferred,
        InvalidScene,
        Failed
    };

    enum class SceneLifecycleEventType : uint8_t
    {
        Loaded,
        Unloaded,
        Reloaded,
        ActiveChanged,
        ExecutionStateChanged
    };

    struct SceneLifecycleEvent
    {
        SceneLifecycleEventType Type = SceneLifecycleEventType::Loaded;
        UUID SceneId;
        SceneExecutionState State = SceneExecutionState::Edit;
    };

    class SceneManager : public Module<SceneManager>
    {
    public:
        using LifecycleListener = std::function<void(const SceneLifecycleEvent&)>;
        using ListenerId = uint64_t;

        class CallbackScope
        {
        public:
            CallbackScope() = default;
            CallbackScope(const CallbackScope&) = delete;
            CallbackScope& operator=(const CallbackScope&) = delete;
            CallbackScope(CallbackScope&& other) noexcept;
            CallbackScope& operator=(CallbackScope&& other) noexcept;
            ~CallbackScope();

        private:
            friend class SceneManager;
            explicit CallbackScope(SceneManager* manager);
            void Release();

            SceneManager* m_Manager = nullptr;
        };

        SceneManager() = default;
        ~SceneManager() = default;

        Ref<Scene> GetActiveScene() const;
        Ref<Scene> GetLoadedScene(const UUID& sceneId) const;
        Vector<UUID> GetLoadedScenes() const;
        const UUID& GetActiveSceneId() const { return m_ActiveSceneId; }
        SceneExecutionState GetExecutionState() const { return m_ExecutionState; }
        const UUID& GetEditSelection() const { return m_EditSelection; }

        void SetActiveScene(const Ref<Scene>& scene);
        void SetActiveScene(const Ref<Scene>& scene, const UUID& sceneId);
        SceneOperationStatus SetActiveScene(const UUID& sceneId);
        SceneOperationStatus LoadScene(const UUID& sceneId, bool makeActive = true);
        SceneOperationStatus UnloadScene(const UUID& sceneId);
        SceneOperationStatus ReloadScene(const UUID& sceneId);

        SceneOperationStatus BeginPlay(const UUID& selectedEntity = UUID::EMPTY);
        SceneOperationStatus BeginSimulation(const UUID& selectedEntity = UUID::EMPTY);
        SceneOperationStatus PausePlay();
        SceneOperationStatus ResumePlay();
        SceneOperationStatus Stop();

        CallbackScope DeferSceneChanges();
        void ProcessDeferredOperations();

        ListenerId AddLifecycleListener(LifecycleListener listener);
        void RemoveLifecycleListener(ListenerId listenerId);
        Vector<SceneLifecycleEvent> DrainManagedLifecycleEvents();

    protected:
        void OnShutdown() override;

    private:
        enum class OperationType : uint8_t
        {
            SetScene,
            SetActive,
            Load,
            Unload,
            Reload,
            BeginPlay,
            BeginSimulation,
            PausePlay,
            ResumePlay,
            Stop
        };

        struct PendingOperation
        {
            OperationType Type = OperationType::SetScene;
            Ref<Scene> SceneRef;
            UUID SceneId;
            UUID Selection;
            bool MakeActive = true;
        };

        SceneOperationStatus Submit(PendingOperation operation);
        SceneOperationStatus Apply(const PendingOperation& operation);
        SceneOperationStatus ApplySetScene(const Ref<Scene>& scene, const UUID& sceneId);
        SceneOperationStatus ApplySetActive(const UUID& sceneId);
        SceneOperationStatus ApplyLoad(const UUID& sceneId, bool makeActive);
        SceneOperationStatus ApplyUnload(const UUID& sceneId);
        SceneOperationStatus ApplyReload(const UUID& sceneId);
        SceneOperationStatus ApplyBeginPlay(const UUID& selection, bool simulate);
        SceneOperationStatus ApplyRuntimeSceneChange(const Ref<Scene>& sourceScene, const UUID& sceneId);
        SceneOperationStatus ApplyPausePlay(bool pause);
        SceneOperationStatus ApplyStop();

        Ref<Scene> DeserializeSceneAsset(const UUID& sceneId) const;
        void SetActiveSceneInternal(const Ref<Scene>& scene, const UUID& sceneId);
        void Emit(SceneLifecycleEventType type, const UUID& sceneId);
        void BeginCallbackDispatch();
        void EndCallbackDispatch();
        void UpdateWindowTitle() const;

        Ref<Scene> m_ActiveScene;
        Ref<Scene> m_EditScene;
        Ref<Scene> m_RuntimeScene;
        UnorderedMap<UUID, Ref<Scene>> m_LoadedScenes;
        UUID m_ActiveSceneId;
        UUID m_EditSceneId;
        UUID m_EditSelection;
        SceneExecutionState m_ExecutionState = SceneExecutionState::Edit;

        UnorderedMap<ListenerId, LifecycleListener> m_LifecycleListeners;
        Vector<SceneLifecycleEvent> m_ManagedLifecycleEvents;
        Deque<PendingOperation> m_PendingOperations;
        ListenerId m_NextListenerId = 1;
        uint32_t m_CallbackDepth = 0;
        bool m_ProcessingOperations = false;
    };
} // namespace Crowny

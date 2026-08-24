#include "cwpch.h"

#include "Crowny/Scene/SceneManager.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Scene/ScriptRuntime.h"
#include "Crowny/Serialization/SceneSerializer.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptSceneManager.h"
#include "Crowny/Window/Window.h"

namespace Crowny
{
    SceneManager::CallbackScope::CallbackScope(SceneManager* manager) : m_Manager(manager)
    {
        if (m_Manager != nullptr)
            m_Manager->BeginCallbackDispatch();
    }

    SceneManager::CallbackScope::CallbackScope(CallbackScope&& other) noexcept : m_Manager(std::exchange(other.m_Manager, nullptr)) {}

    SceneManager::CallbackScope& SceneManager::CallbackScope::operator=(CallbackScope&& other) noexcept
    {
        if (this == &other)
            return *this;
        Release();
        m_Manager = std::exchange(other.m_Manager, nullptr);
        return *this;
    }

    SceneManager::CallbackScope::~CallbackScope() { Release(); }

    void SceneManager::CallbackScope::Release()
    {
        if (m_Manager == nullptr)
            return;
        m_Manager->EndCallbackDispatch();
        m_Manager = nullptr;
    }

    void SceneManager::OnShutdown()
    {
        ApplyStop();
        m_ActiveScene = nullptr;
        m_EditScene = nullptr;
        m_RuntimeScene = nullptr;
        m_LoadedScenes.clear();
        m_PendingOperations.clear();
        m_LifecycleListeners.clear();
        m_ManagedLifecycleEvents.clear();
    }

    Ref<Scene> SceneManager::GetActiveScene() const { return m_ActiveScene; }

    Ref<Scene> SceneManager::GetLoadedScene(const UUID& sceneId) const
    {
        const auto iter = m_LoadedScenes.find(sceneId);
        return iter != m_LoadedScenes.end() ? iter->second : nullptr;
    }

    Vector<UUID> SceneManager::GetLoadedScenes() const
    {
        Vector<UUID> scenes;
        scenes.reserve(m_LoadedScenes.size());
        for (const auto& [sceneId, scene] : m_LoadedScenes)
            scenes.push_back(sceneId);
        std::sort(scenes.begin(), scenes.end());
        return scenes;
    }

    void SceneManager::SetActiveScene(const Ref<Scene>& scene) { SetActiveScene(scene, UUID::EMPTY); }

    void SceneManager::SetActiveScene(const Ref<Scene>& scene, const UUID& sceneId)
    {
        PendingOperation operation;
        operation.Type = OperationType::SetScene;
        operation.SceneRef = scene;
        operation.SceneId = sceneId;
        Submit(std::move(operation));
    }

    SceneOperationStatus SceneManager::SetActiveScene(const UUID& sceneId)
    {
        PendingOperation operation;
        operation.Type = OperationType::SetActive;
        operation.SceneId = sceneId;
        return Submit(std::move(operation));
    }

    SceneOperationStatus SceneManager::LoadScene(const UUID& sceneId, bool makeActive)
    {
        PendingOperation operation;
        operation.Type = OperationType::Load;
        operation.SceneId = sceneId;
        operation.MakeActive = makeActive;
        return Submit(std::move(operation));
    }

    SceneOperationStatus SceneManager::UnloadScene(const UUID& sceneId)
    {
        PendingOperation operation;
        operation.Type = OperationType::Unload;
        operation.SceneId = sceneId;
        return Submit(std::move(operation));
    }

    SceneOperationStatus SceneManager::ReloadScene(const UUID& sceneId)
    {
        PendingOperation operation;
        operation.Type = OperationType::Reload;
        operation.SceneId = sceneId;
        return Submit(std::move(operation));
    }

    SceneOperationStatus SceneManager::BeginPlay(const UUID& selectedEntity)
    {
        PendingOperation operation;
        operation.Type = OperationType::BeginPlay;
        operation.Selection = selectedEntity;
        return Submit(std::move(operation));
    }

    SceneOperationStatus SceneManager::BeginSimulation(const UUID& selectedEntity)
    {
        PendingOperation operation;
        operation.Type = OperationType::BeginSimulation;
        operation.Selection = selectedEntity;
        return Submit(std::move(operation));
    }

    SceneOperationStatus SceneManager::PausePlay()
    {
        PendingOperation operation;
        operation.Type = OperationType::PausePlay;
        return Submit(std::move(operation));
    }

    SceneOperationStatus SceneManager::ResumePlay()
    {
        PendingOperation operation;
        operation.Type = OperationType::ResumePlay;
        return Submit(std::move(operation));
    }

    SceneOperationStatus SceneManager::Stop()
    {
        PendingOperation operation;
        operation.Type = OperationType::Stop;
        return Submit(std::move(operation));
    }

    SceneManager::CallbackScope SceneManager::DeferSceneChanges() { return CallbackScope(this); }

    SceneOperationStatus SceneManager::Submit(PendingOperation operation)
    {
        if (m_CallbackDepth > 0 || m_ProcessingOperations)
        {
            m_PendingOperations.push_back(std::move(operation));
            return SceneOperationStatus::Deferred;
        }

        m_ProcessingOperations = true;
        const SceneOperationStatus result = Apply(operation);
        while (!m_PendingOperations.empty())
        {
            PendingOperation pending = std::move(m_PendingOperations.front());
            m_PendingOperations.pop_front();
            Apply(pending);
        }
        m_ProcessingOperations = false;
        return result;
    }

    void SceneManager::ProcessDeferredOperations()
    {
        if (m_CallbackDepth > 0 || m_ProcessingOperations || m_PendingOperations.empty())
            return;

        m_ProcessingOperations = true;
        while (!m_PendingOperations.empty())
        {
            PendingOperation operation = std::move(m_PendingOperations.front());
            m_PendingOperations.pop_front();
            Apply(operation);
        }
        m_ProcessingOperations = false;
    }

    SceneOperationStatus SceneManager::Apply(const PendingOperation& operation)
    {
        switch (operation.Type)
        {
        case OperationType::SetScene: return ApplySetScene(operation.SceneRef, operation.SceneId);
        case OperationType::SetActive: return ApplySetActive(operation.SceneId);
        case OperationType::Load: return ApplyLoad(operation.SceneId, operation.MakeActive);
        case OperationType::Unload: return ApplyUnload(operation.SceneId);
        case OperationType::Reload: return ApplyReload(operation.SceneId);
        case OperationType::BeginPlay: return ApplyBeginPlay(operation.Selection, false);
        case OperationType::BeginSimulation: return ApplyBeginPlay(operation.Selection, true);
        case OperationType::PausePlay: return ApplyPausePlay(true);
        case OperationType::ResumePlay: return ApplyPausePlay(false);
        case OperationType::Stop: return ApplyStop();
        }
        return SceneOperationStatus::Failed;
    }

    SceneOperationStatus SceneManager::ApplySetScene(const Ref<Scene>& scene, const UUID& sceneId)
    {
        ApplyStop();
        m_EditSelection = UUID::EMPTY;
        if (!sceneId.Empty() && scene != nullptr)
        {
            m_LoadedScenes[sceneId] = scene;
            Emit(SceneLifecycleEventType::Loaded, sceneId);
        }
        SetActiveSceneInternal(scene, sceneId);
        return scene != nullptr ? SceneOperationStatus::Completed : SceneOperationStatus::InvalidScene;
    }

    SceneOperationStatus SceneManager::ApplySetActive(const UUID& sceneId)
    {
        const auto iter = m_LoadedScenes.find(sceneId);
        if (iter == m_LoadedScenes.end())
            return SceneOperationStatus::InvalidScene;
        if (m_ExecutionState != SceneExecutionState::Edit)
            return ApplyRuntimeSceneChange(iter->second, sceneId);
        ApplyStop();
        m_EditSelection = UUID::EMPTY;
        SetActiveSceneInternal(iter->second, sceneId);
        return SceneOperationStatus::Completed;
    }

    SceneOperationStatus SceneManager::ApplyLoad(const UUID& sceneId, bool makeActive)
    {
        if (sceneId.Empty())
            return SceneOperationStatus::InvalidScene;

        Ref<Scene> scene = GetLoadedScene(sceneId);
        if (scene == nullptr)
        {
            scene = DeserializeSceneAsset(sceneId);
            if (scene == nullptr)
                return SceneOperationStatus::Failed;
            m_LoadedScenes[sceneId] = scene;
            Emit(SceneLifecycleEventType::Loaded, sceneId);
        }

        if (makeActive)
        {
            if (m_ExecutionState != SceneExecutionState::Edit)
                return ApplyRuntimeSceneChange(scene, sceneId);
            ApplyStop();
            m_EditSelection = UUID::EMPTY;
            SetActiveSceneInternal(scene, sceneId);
        }
        return SceneOperationStatus::Completed;
    }

    SceneOperationStatus SceneManager::ApplyUnload(const UUID& sceneId)
    {
        const auto iter = m_LoadedScenes.find(sceneId);
        if (iter == m_LoadedScenes.end())
            return SceneOperationStatus::InvalidScene;
        if (m_ExecutionState != SceneExecutionState::Edit && sceneId == m_EditSceneId)
            return SceneOperationStatus::Failed;

        if (m_ActiveSceneId == sceneId)
        {
            ApplyStop();
            if (m_ActiveSceneId == sceneId)
            {
                m_EditSelection = UUID::EMPTY;
                SetActiveSceneInternal(nullptr, UUID::EMPTY);
            }
        }
        m_LoadedScenes.erase(iter);
        Emit(SceneLifecycleEventType::Unloaded, sceneId);
        return SceneOperationStatus::Completed;
    }

    SceneOperationStatus SceneManager::ApplyReload(const UUID& sceneId)
    {
        if (m_LoadedScenes.find(sceneId) == m_LoadedScenes.end())
            return SceneOperationStatus::InvalidScene;

        Ref<Scene> replacement = DeserializeSceneAsset(sceneId);
        if (replacement == nullptr)
            return SceneOperationStatus::Failed;

        const bool wasActive = m_ActiveSceneId == sceneId;
        const bool wasExecuting = wasActive && m_ExecutionState != SceneExecutionState::Edit;
        m_LoadedScenes[sceneId] = replacement;
        if (wasExecuting)
            ApplyRuntimeSceneChange(replacement, sceneId);
        else if (wasActive)
            SetActiveSceneInternal(replacement, sceneId);
        Emit(SceneLifecycleEventType::Reloaded, sceneId);
        return SceneOperationStatus::Completed;
    }

    SceneOperationStatus SceneManager::ApplyBeginPlay(const UUID& selection, bool simulate)
    {
        if (m_ActiveScene == nullptr)
            return SceneOperationStatus::InvalidScene;
        if (m_ExecutionState != SceneExecutionState::Edit)
            ApplyStop();

        m_EditScene = m_ActiveScene;
        m_EditSceneId = m_ActiveSceneId;
        m_EditSelection = selection;
        m_RuntimeScene = CreateRef<Scene>(*m_EditScene);
        m_RuntimeScene->SetEditorScene(false);
        m_ActiveScene = m_RuntimeScene;
        m_ExecutionState = simulate ? SceneExecutionState::Simulate : SceneExecutionState::Play;
        UpdateWindowTitle();

        if (simulate)
            m_RuntimeScene->OnSimulationStart();
        else
        {
            m_RuntimeScene->OnRuntimeStart();
            ScriptRuntime::OnStart(m_RuntimeScene);
        }
        Emit(SceneLifecycleEventType::ActiveChanged, m_EditSceneId);
        Emit(SceneLifecycleEventType::ExecutionStateChanged, m_EditSceneId);
        return SceneOperationStatus::Completed;
    }

    SceneOperationStatus SceneManager::ApplyPausePlay(bool pause)
    {
        if (pause && m_ExecutionState != SceneExecutionState::Play)
            return SceneOperationStatus::Failed;
        if (!pause && m_ExecutionState != SceneExecutionState::PlayPaused)
            return SceneOperationStatus::Failed;

        if (pause)
        {
            m_RuntimeScene->OnRuntimePause();
            m_ExecutionState = SceneExecutionState::PlayPaused;
        }
        else
        {
            m_RuntimeScene->OnRuntimeResume();
            m_ExecutionState = SceneExecutionState::Play;
        }
        Emit(SceneLifecycleEventType::ExecutionStateChanged, m_EditSceneId);
        return SceneOperationStatus::Completed;
    }

    SceneOperationStatus SceneManager::ApplyRuntimeSceneChange(const Ref<Scene>& sourceScene, const UUID& sceneId)
    {
        if (sourceScene == nullptr || m_RuntimeScene == nullptr || m_ExecutionState == SceneExecutionState::Edit)
            return SceneOperationStatus::InvalidScene;

        const SceneExecutionState previousState = m_ExecutionState;
        if (previousState == SceneExecutionState::Simulate)
            m_RuntimeScene->OnSimulationEnd();
        else
        {
            ScriptRuntime::OnShutdown(m_RuntimeScene);
            m_RuntimeScene->OnRuntimeStop();
        }

        m_RuntimeScene = CreateRef<Scene>(*sourceScene);
        m_RuntimeScene->SetEditorScene(false);
        m_ActiveScene = m_RuntimeScene;
        m_ActiveSceneId = sceneId;
        if (previousState == SceneExecutionState::Simulate)
            m_RuntimeScene->OnSimulationStart();
        else
        {
            m_ExecutionState = SceneExecutionState::Play;
            m_RuntimeScene->OnRuntimeStart();
            ScriptRuntime::OnStart(m_RuntimeScene);
            if (previousState == SceneExecutionState::PlayPaused)
            {
                m_RuntimeScene->OnRuntimePause();
                m_ExecutionState = SceneExecutionState::PlayPaused;
            }
        }
        UpdateWindowTitle();
        Emit(SceneLifecycleEventType::ActiveChanged, sceneId);
        return SceneOperationStatus::Completed;
    }

    SceneOperationStatus SceneManager::ApplyStop()
    {
        if (m_ExecutionState == SceneExecutionState::Edit)
            return SceneOperationStatus::Completed;

        Ref<Scene> runtimeScene = m_RuntimeScene;
        if (m_ExecutionState == SceneExecutionState::Simulate)
            runtimeScene->OnSimulationEnd();
        else
        {
            ScriptRuntime::OnShutdown(runtimeScene);
            runtimeScene->OnRuntimeStop();
        }

        m_ActiveScene = m_EditScene;
        m_ActiveSceneId = m_EditSceneId;
        m_RuntimeScene = nullptr;
        m_EditScene = nullptr;
        m_EditSceneId = UUID::EMPTY;
        m_ExecutionState = SceneExecutionState::Edit;
        UpdateWindowTitle();
        Emit(SceneLifecycleEventType::ActiveChanged, m_ActiveSceneId);
        Emit(SceneLifecycleEventType::ExecutionStateChanged, m_ActiveSceneId);
        return SceneOperationStatus::Completed;
    }

    Ref<Scene> SceneManager::DeserializeSceneAsset(const UUID& sceneId) const
    {
        if (AssetManager::TryGet() == nullptr)
            return nullptr;
        Path scenePath;
        if (!AssetManager::TryGet()->GetAssetPath(sceneId, scenePath))
            return nullptr;

        Ref<Scene> scene = CreateRef<Scene>(false);
        SceneSerializer serializer(scene);
        if (!serializer.Deserialize(scenePath))
            return nullptr;
        scene->SetEditorScene(false);
        return scene;
    }

    void SceneManager::SetActiveSceneInternal(const Ref<Scene>& scene, const UUID& sceneId)
    {
        if (m_ActiveScene == scene && m_ActiveSceneId == sceneId)
            return;
        m_ActiveScene = scene;
        m_ActiveSceneId = scene != nullptr ? sceneId : UUID::EMPTY;
        UpdateWindowTitle();
        Emit(SceneLifecycleEventType::ActiveChanged, m_ActiveSceneId);
    }

    SceneManager::ListenerId SceneManager::AddLifecycleListener(LifecycleListener listener)
    {
        if (!listener)
            return 0;
        const ListenerId id = m_NextListenerId++;
        m_LifecycleListeners.emplace(id, std::move(listener));
        return id;
    }

    void SceneManager::RemoveLifecycleListener(ListenerId listenerId) { m_LifecycleListeners.erase(listenerId); }

    Vector<SceneLifecycleEvent> SceneManager::DrainManagedLifecycleEvents()
    {
        Vector<SceneLifecycleEvent> events;
        events.swap(m_ManagedLifecycleEvents);
        return events;
    }

    void SceneManager::Emit(SceneLifecycleEventType type, const UUID& sceneId)
    {
        const SceneLifecycleEvent event{ type, sceneId, m_ExecutionState };
        if (m_ManagedLifecycleEvents.size() == 256)
            m_ManagedLifecycleEvents.erase(m_ManagedLifecycleEvents.begin());
        m_ManagedLifecycleEvents.push_back(event);

        Vector<LifecycleListener> listeners;
        listeners.reserve(m_LifecycleListeners.size());
        for (const auto& [id, listener] : m_LifecycleListeners)
            listeners.push_back(listener);

        {
            CallbackScope scope(this);
            for (const LifecycleListener& listener : listeners)
                listener(event);
        }
        ScriptSceneManager::DispatchPendingEvents();
    }

    void SceneManager::BeginCallbackDispatch() { ++m_CallbackDepth; }

    void SceneManager::EndCallbackDispatch()
    {
        CW_ENGINE_ASSERT(m_CallbackDepth > 0);
        --m_CallbackDepth;
        if (m_CallbackDepth == 0)
            ProcessDeferredOperations();
    }

    void SceneManager::UpdateWindowTitle() const
    {
        Application* application = Application::TryGet();
        if (application == nullptr || !Application::IsStartedUp() || application->GetApplicationDesc().Headless)
            return;
        String title = application->GetApplicationDesc().Name;
        if (m_ActiveScene != nullptr)
            title += " - " + m_ActiveScene->GetName();
        application->GetWindow().SetTitle(title);
    }
} // namespace Crowny

#pragma once

#include "Crowny/Application/Application.h"
#include "Crowny/Renderer/EditorCamera.h"
#include "Crowny/Renderer/RenderSnapshot.h"

#include "Editor/UndoRedo.h"

#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scripting/ManagedReload.h"

#include <chrono>

namespace Crowny
{
    class TextureEditor;
    class ViewportPanel;
    class InspectorPanel;
    class OpenGLInformationPanel;
    class AssetBrowserPanel;
    class HierarchyPanel;
    class ConsolePanel;
    class AudioMixerPanel;
#ifdef CW_WITH_NODES
    class NodeEditorPanel;
#endif
    class RenderTarget;
    class RenderTexture;
    class ImGuiMenuBar;
    class EditorPanelRegistry;
    class Scene;

    class EditorLayer : public Layer
    {
    public:
        EditorLayer();
        virtual ~EditorLayer();

        virtual void OnAttach() override;
        virtual void OnDetach() override;

        virtual void OnUpdate(Timestep ts) override;
        virtual void OnImGuiRender() override;
        virtual void OnEvent(Event& e) override;

        bool OnKeyPressed(KeyPressedEvent& e);
        bool OnMouseButtonPressed(MouseButtonPressedEvent& e);
        bool OnViewportEvent(Event& event);

        bool RebuildAssemblies();
        void BuildGame();

        void CreateNewScene();
        void OpenScene();
        void OpenScene(const UUID& sceneId);
        void OpenScene(const Path& filepath);
        void SaveActiveScene();
        void SaveActiveSceneAs();
        void AddRecentScene(const UUID& sceneId);

    private:
        void ExecuteProjectAssetRefresh();
        Entity PickEntity();
        Entity PickEntity(const glm::vec2& screenPosition);
        void CreateRenderTarget();
        void HandleRenderTargetResize();
        void HandleSceneState(Timestep ts);
        RenderSnapshot& AcquireSnapshot();
        void SubmitSnapshot(RenderSnapshot& snapshot);
        void SetupImGuiRender();

        void RenderOverlay();
        bool SynchronizeActiveSceneAsset(const Ref<Scene>& scene);
        void AddRecentEntry(const Path& path);
        void SetProjectSettings();
        void SaveProjectSettings();
        void ApplyEditorSettings();
        void FinishDeferredStartup();

        void UI_ProjectManager();
        void UI_Header();
        void UI_ViewportSettings();
        void UI_Settings();
        void UI_Physics2DSettings();
        void UI_TimeSettings();
        void UI_BuildGame();
        void UI_CommandPalette();
        void UI_Notifications();
        void UI_ScriptInfo();
        void UI_AssetInfo();
        void UI_EntityDebugInfo();

        void TogglePlay();
        void ToggleSimulation();
        void TogglePause();
        void ResetWorkspaceLayout();

        enum class NotificationKind
        {
            Info,
            Success,
            Error
        };
        void AddNotification(const String& message, NotificationKind kind = NotificationKind::Info);

    private:
        friend class Time;

        Scope<EditorPanelRegistry> m_Panels;
        Scope<ImGuiMenuBar> m_MenuBar;
        Ref<Scene> m_Temp;
        UUID m_TempSceneId;
        Ref<RenderTexture> m_RenderTarget;
        Ref<RenderTarget> m_ResizedRenderTarget;
        RenderSnapshot m_FallbackSnapshot;

        InspectorPanel* m_InspectorPanel = nullptr;
        HierarchyPanel* m_HierarchyPanel = nullptr;
        ViewportPanel* m_ViewportPanel = nullptr;
        ConsolePanel* m_ConsolePanel = nullptr;
        AssetBrowserPanel* m_AssetBrowser = nullptr;
        AudioMixerPanel* m_AudioMixerPanel = nullptr;
#ifdef CW_WITH_NODES
        NodeEditorPanel* m_NodeEditorPanel = nullptr;
#endif

        bool m_ShowSettings = false;
        bool m_ShowBuildWindow = false;
        bool m_ShowCommandPalette = false;
        bool m_FocusCommandPalette = false;
        bool m_ResetLayoutRequested = false;
        bool m_WasImporting = false;
        bool m_ShowDemoWindow = false;
        bool m_ShowColliders = false;
        bool m_AutoLoadLastProject = false;
        bool m_ShowScriptDebugInfo = false;
        bool m_ShowAssetInfo = false;
        bool m_ShowEmptyMetadataAssetInfo = false;
        bool m_ShowEntityDebugInfo = false;
        bool m_DeferredStartupPending = true;
        uint32_t m_StartupFrameCount = 0;
        Path m_PendingProjectPath;

        // Viewport settings
        bool m_WireframeMode = false;
        bool m_ShowRenderingStatistics = true;
        bool m_ShowGrid = true;
        bool m_ShowGridAxes = true;
        float m_GridFineSize = 1.0f;
        float m_GridCoarseSize = 10.0f;
        float m_GridLineWidth = 0.02f;
        float m_GridOpacity = 0.4f;
        glm::vec4 m_ColliderColor = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
        bool m_ShowViewportSettings = false;

        static EditorCamera s_EditorCamera;
        bool m_CreatingNewProject = false;
        bool m_GameMode = false;
        bool m_OpenProject = false;
        bool m_NewProject = false;
        String m_NewProjectPath;
        String m_NewProjectName;
        glm::vec2 m_ViewportSize = { 1280.0f, 720.0f }; // and dis

        enum class HubPage
        {
            RecentProjects,
            NewProject
        };
        HubPage m_HubPage = HubPage::RecentProjects;
        int m_SelectedRecentIdx = -1;
        String m_RecentSearchFilter;
        String m_SettingsSearch;
        String m_CommandSearch;

        enum class BuildStatus
        {
            Ready,
            Running,
            Succeeded,
            Failed
        };
        BuildStatus m_BuildStatus = BuildStatus::Ready;
        float m_BuildProgress = 0.0f;
        String m_BuildResult;

        struct Notification
        {
            uint32_t Id = 0;
            String Message;
            NotificationKind Kind = NotificationKind::Info;
            float SecondsLeft = 0.0f; // Counts down while the card is not hovered; the card fades out at the end.
        };
        Vector<Notification> m_Notifications;
        uint32_t m_NextNotificationId = 1;

        Mutex m_FileWatchMutex;
        Vector<Path> m_FileWatchQueue;
        ManagedReloadDebouncer m_AssemblyReloadDebouncer;
        uint64_t m_ManagedBuildGeneration = 0;

        Stack<UndoAction> m_UndoStack;

        int32_t m_VisualStudioVersionId = 0;

        enum class SceneState
        {
            Edit = 0,
            Play = 1,
            Simulate = 2,
            PausePlay = 3
        };

        SceneState m_SceneState = SceneState::Edit;
        SceneManager::ListenerId m_SceneLifecycleListener = 0;
        SceneRenderer* m_SceneRenderer;

    public:
        static EditorCamera& GetEditorCamera() { return s_EditorCamera; }

        // Viewport render-overlay state shared between Settings > Viewport, the viewport settings popup
        // and the viewport's top-right overlay toolbar.
        bool IsWireframeMode() const { return m_WireframeMode; }
        void SetWireframeMode(bool wireframe) { m_WireframeMode = wireframe; }
        bool IsShowingRenderingStatistics() const { return m_ShowRenderingStatistics; }
        void SetShowRenderingStatistics(bool show) { m_ShowRenderingStatistics = show; }
    };
} // namespace Crowny

#pragma once

#include "Crowny/Renderer/EditorCamera.h"

#include "Crowny/Common/Time.h"

namespace filewatch
{
    template <typename T> class FileWatch;
}

namespace Crowny
{
    class TextureEditor;
    class ViewportPanel;
    class InspectorPanel;
    class OpenGLInformationPanel;
    class AssetBrowserPanel;
    class HierarchyPanel;
    class ConsolePanel;
    class RenderTarget;
    class ImGuiMenuBar;
    class ImGuiPanel;
    class Scene;

    class EditorLayer : public Layer
    {
    public:
        EditorLayer();
        virtual ~EditorLayer() = default;

        virtual void OnAttach() override;
        virtual void OnDetach() override;

        virtual void OnUpdate(Timestep ts) override;
        virtual void OnImGuiRender() override;
        virtual void OnEvent(Event& e) override;

        bool OnKeyPressed(KeyPressedEvent& e);
        bool OnMouseButtonPressed(MouseButtonPressedEvent& e);
        bool OnViewportEvent(Event& event);

        void RebuildAssemblies(Event& event);
        void BuildGame(Event& event);

        void CreateNewScene();
        void OpenScene();
        void OpenScene(const Path& filepath);
        void SaveActiveScene();
        void SaveActiveSceneAs();

    private:
        void RenderOverlay();
        void AddRecentEntry(const Path& path);
        void SetProjectSettings();
        void SaveProjectSettings();

        void UI_Header();
        void UI_GizmoSettings();
        void UI_Settings();
        void UI_Physics2DSettings();
        void UI_ScriptInfo();
        void UI_AssetInfo();
        void UI_EntityDebugInfo();

    private:
        friend class Time;

        ImGuiMenuBar* m_MenuBar = nullptr;
        Ref<Scene> m_Temp;
        Ref<RenderTarget> m_RenderTarget;
        Ref<RenderTarget> m_ResizedRenderTarget;

        ImGuiPanel* m_GLInfoPanel = nullptr;
        InspectorPanel* m_InspectorPanel = nullptr;
        HierarchyPanel* m_HierarchyPanel = nullptr;
        ViewportPanel* m_ViewportPanel = nullptr;
        TextureEditor* m_TextureEditor = nullptr;
        ConsolePanel* m_ConsolePanel = nullptr;
        AssetBrowserPanel* m_AssetBrowser = nullptr;

        bool m_ShowDemoWindow = false;
        bool m_ShowColliders = false;
        bool m_AutoLoadLastProject = false;
        bool m_ShowScriptDebugInfo = false;
        bool m_ShowAssetInfo = false;
        bool m_ShowEmptyMetadataAssetInfo = false;
        bool m_ShowEntityDebugInfo = false;

        Vector<ImGuiPanel*> m_ImGuiWindows;
        static EditorCamera s_EditorCamera;
        Entity m_HoveredEntity;
        bool m_CreatingNewProject = false;
        bool m_GameMode = false;
        bool m_OpenProject = false;
        bool m_NewProject = false;
        String m_NewProjectPath;
        String m_NewProjectName;
        glm::vec2 m_ViewportSize = { 1280.0f, 720.0f }; // and dis

        Scope<filewatch::FileWatch<Path>> m_Watch;

        static float s_DeltaTime;
        static float s_SmoothDeltaTime;
        static float s_RealtimeSinceStartup;
        static float s_Time;
        static float s_FixedDeltaTime;
        static float s_FrameCount;

        enum class SceneState
        {
            Edit = 0,
            Play = 1,
            Simulate = 2,
            PausePlay = 3
        };

        SceneState m_SceneState = SceneState::Edit;

    public:
        static EditorCamera& GetEditorCamera() { return s_EditorCamera; }
    };
} // namespace Crowny
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

    class UndoAction
    {
    public:
        virtual void Commit() {}
        virtual void Revert() {}
    };

    class UndoRedo : public Module<UndoRedo>
    {
    public:
        void RegisterAction(const Ref<UndoAction>& action) { m_UndoStack.push_back(action); }
        void Undo()
        {
            if (m_UndoStack.empty())
                return;
            m_UndoStack.back()->Revert();
            m_RedoStack.push_back(m_UndoStack.back());
            m_UndoStack.pop_back();
        }

        void Redo()
        {
            if (m_RedoStack.empty())
                return;
            m_RedoStack.back()->Commit();
            m_UndoStack.push_back(m_RedoStack.back());
            m_RedoStack.pop_back();
        }

    private:
        Vector<Ref<UndoAction>> m_UndoStack;
        Vector<Ref<UndoAction>> m_RedoStack;
    };

    template <typename T> class AddComponentAction : public UndoAction
    {
    public:
        AddComponentAction(Entity entity) : m_Entity(entity) {}

        virtual void Commit() override { m_Entity.AddComponent<T>(); }

        virtual void Revert() override { m_Entity.RemoveComponent<T>(); }

    private:
        Entity m_Entity;
    };

    template <typename T> class RemoveComponentAction : public UndoAction
    {
    public:
        RemoveComponentAction(Entity entity, const T& component) : m_Entity(entity), m_Component(component) {}

        virtual void Commit() override { m_Entity.RemoveComponent<T>(); }

        virtual void Revert() override { m_Entity.AddComponent<T>(m_Component); }

    private:
        Entity m_Entity;
        T m_Component;
    };

    template <typename T> class ChangeComponentAction : public UndoAction
    {
        ChangeComponentAction(Entity entity, const T& oldComponent) : m_Entity(entity), m_OldComponent(oldComponent) {}

        virtual void Commit() override { m_Entity.AddOrReplaceComponent<T>(m_NewComponent); }

        virtual void Revert() override
        {
            m_NewComponent = m_Entity.GetComponent()<T>();
            m_Entity.AddOrReplaceComponent<T>(m_OldComponent);
        }

    private:
        Entity m_Entity;
        T m_OldComponent;
        T m_NewComponent;
    };

    class EntityCreatedAction : public UndoAction
    {
    };

    class EntityDeletedAction : public UndoAction
    {
    public:
        EntityDeletedAction(Entity e) {}

    private:
        entt::registry m_ComponentRegistry;
    };

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
        void ExecuteProjectAssetRefresh();
        void HandleMousePicking();
        void CreateRenderTarget();
        void HandleRenderTargetResize();
        void HandleSceneState(Timestep ts);
        void SetupImGuiRender();

        void RenderOverlay();
        void AddRecentEntry(const Path& path);
        void SetProjectSettings();
        void SaveProjectSettings();
        void ApplyEditorSettings();

        void UI_ProjectManager();
        void UI_Header();
        void UI_GizmoSettings();
        void UI_Settings();
        void UI_Physics2DSettings();
        void UI_TimeSettings();
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

        bool m_ShowTimeSettings = false;
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
        Mutex m_FileWatchMutex;
        Vector<Path> m_FileWatchQueue;

        Stack<UndoAction> m_UndoStack;

        int32_t m_VisualStudioVersionId = 0;

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
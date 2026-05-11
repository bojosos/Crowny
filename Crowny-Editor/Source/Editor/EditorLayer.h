#pragma once

#include "Crowny/Application/Application.h"
#include "Crowny/Renderer/EditorCamera.h"
#include "Crowny/Renderer/RenderSnapshot.h"

#include "Crowny/Common/Time.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Serialization/SceneSerializer.h"
#include <imgui.h>
#include <yaml-cpp/yaml.h>

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
    class ImGuiMenuBar;
    class ImGuiPanel;
    class Scene;

    class UndoAction : public RefCounted
    {
    public:
        virtual void Commit() {}
        virtual void Revert() {}
    };

    class UndoRedo : public Module<UndoRedo>
    {
    public:
        void RegisterAction(const Ref<UndoAction>& action)
        {
            m_RedoStack.clear();
            m_UndoStack.push_back(action);
        }
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

        void BeginComponentScope(std::function<Ref<UndoAction>()> factory)
        {
            // Only capture the snapshot at the start of an interaction. If a drag is
            // already in progress we must keep the original pre-edit factory so that
            // the undo action stores the value from *before* the user started dragging,
            // not the already-modified value from the current frame.
            if (!m_InInteraction)
                m_Factory = std::move(factory);
        }
        void EndComponentScope()
        {
            // Don't tear down state mid-interaction; the factory and flag must survive
            // until OnItemInteract sees IsItemDeactivatedAfterEdit().
            if (!m_InInteraction)
                m_Factory = nullptr;
        }

        void OnItemInteract()
        {
            if (!m_Factory)
                return;
            if (ImGui::IsItemActivated())
                m_InInteraction = true;
            if (m_InInteraction && ImGui::IsItemDeactivatedAfterEdit())
            {
                RegisterAction(m_Factory());
                m_InInteraction = false;
            }
        }

    private:
        Vector<Ref<UndoAction>> m_UndoStack;
        Vector<Ref<UndoAction>> m_RedoStack;
        std::function<Ref<UndoAction>()> m_Factory;
        bool m_InInteraction = false;
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
    public:
        ChangeComponentAction(Entity entity, const T& oldComp, const T& newComp) : m_Entity(entity), m_OldComponent(oldComp), m_NewComponent(newComp)
        {
        }

        void Commit() override { m_Entity.AddOrReplaceComponent<T>(m_NewComponent); }
        void Revert() override { m_Entity.AddOrReplaceComponent<T>(m_OldComponent); }

    private:
        Entity m_Entity;
        T m_OldComponent, m_NewComponent;
    };

    class EntityCreatedAction : public UndoAction
    {
    public:
        EntityCreatedAction(Entity e) : m_Uuid(e.GetUuid()), m_Name(e.GetName()), m_ParentUuid(e.GetParent().GetUuid()) {}

        void Commit() override // redo: recreate
        {
            auto scene = gSceneManager->GetActiveScene();
            Entity e = scene->CreateEntityWithUuid(m_Uuid, m_Name);
            Entity parent = scene->GetEntityFromUuid(m_ParentUuid);
            if (parent)
                parent.AddChild(e);
        }
        void Revert() override // undo: destroy
        {
            auto scene = gSceneManager->GetActiveScene();
            Entity e = scene->GetEntityFromUuid(m_Uuid);
            if (e)
                scene->DestroyEntity(e);
        }

    private:
        UUID m_Uuid;
        String m_Name;
        UUID m_ParentUuid;
    };

    // TODO: Try to copy in a temp registry.
    class EntityDeletedAction : public UndoAction
    {
    public:
        EntityDeletedAction(Entity entity, const Ref<Scene>& scene)
          : m_Scene(scene), m_Uuid(entity.GetUuid()), m_ParentUuid(entity.GetParent().GetUuid())
        {
            YAML::Emitter out;
            out << YAML::BeginSeq;
            SceneSerializer serializer(scene);
            serializer.SerializeEntity(out, entity);
            out << YAML::EndSeq;
            m_Yaml = out.c_str();
        }
        void Commit() override // redo: destroy again
        {
            Entity e = m_Scene->GetEntityFromUuid(m_Uuid);
            if (e)
                m_Scene->DestroyEntity(e);
        }
        void Revert() override // undo: recreate from YAML
        {
            YAML::Node node = YAML::Load(m_Yaml);
            SceneSerializer serializer(m_Scene);
            serializer.DeserializeEntities(node);
            Entity restored = m_Scene->GetEntityFromUuid(m_Uuid);
            Entity parent = m_Scene->GetEntityFromUuid(m_ParentUuid);
            if (restored && parent)
                parent.AddChild(restored);
        }

    private:
        Ref<Scene> m_Scene;
        UUID m_Uuid, m_ParentUuid;
        String m_Yaml;
    };

    class EntityReparentAction : public UndoAction
    {
    public:
        EntityReparentAction(Entity e, Entity oldParent, Entity newParent)
          : m_Entity(e.GetUuid()), m_OldParent(oldParent.GetUuid()), m_NewParent(newParent.GetUuid())
        {
        }

        void Commit() override { Reparent(m_NewParent); }
        void Revert() override { Reparent(m_OldParent); }

    private:
        void Reparent(UUID targetUuid)
        {
            auto scene = gSceneManager->GetActiveScene();
            scene->GetEntityFromUuid(m_Entity).SetParent(scene->GetEntityFromUuid(targetUuid));
        }
        UUID m_Entity, m_OldParent, m_NewParent;
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

        void RebuildAssemblies();
        void RebuildAssemblies(Event& event) { RebuildAssemblies(); }
        void BuildGame(Event& event);

        void CreateNewScene();
        void OpenScene();
        void OpenScene(const Path& filepath);
        void SaveActiveScene();
        void SaveActiveSceneAs();
        void AddRecentScene(const Path& path);

    private:
        void ExecuteProjectAssetRefresh();
        Entity PickEntity();
        Entity PickEntity(const glm::vec2& coords);
        void CreateRenderTarget();
        void HandleRenderTargetResize();
        void HandleSceneState(Timestep ts);
        void SubmitSnapshot(RenderSnapshot&& snapshot);
        void SetupImGuiRender();

        void RenderOverlay();
        void AddRecentEntry(const Path& path);
        void SetProjectSettings();
        void SaveProjectSettings();
        void ApplyEditorSettings();

        void UI_ProjectManager();
        void UI_Header();
        void UI_GizmoSettings();
        void UI_ViewportSettings();
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
        Ref<Scene> m_EditorSceneBackup; // Snapshot of the editor scene before entering play/simulate
        Ref<RenderTarget> m_RenderTarget;
        Ref<RenderTarget> m_ResizedRenderTarget;

        ImGuiPanel* m_GLInfoPanel = nullptr;
        InspectorPanel* m_InspectorPanel = nullptr;
        HierarchyPanel* m_HierarchyPanel = nullptr;
        ViewportPanel* m_ViewportPanel = nullptr;
        TextureEditor* m_TextureEditor = nullptr;
        ConsolePanel* m_ConsolePanel = nullptr;
        AssetBrowserPanel* m_AssetBrowser = nullptr;
        AudioMixerPanel* m_AudioMixerPanel = nullptr;
#ifdef CW_WITH_NODES
        NodeEditorPanel* m_NodeEditorPanel = nullptr;
#endif

        bool m_ShowTimeSettings = false;
        bool m_ShowDemoWindow = false;
        bool m_ShowColliders = false;
        bool m_AutoLoadLastProject = false;
        bool m_ShowScriptDebugInfo = false;
        bool m_ShowAssetInfo = false;
        bool m_ShowEmptyMetadataAssetInfo = false;
        bool m_ShowEntityDebugInfo = false;

        // Viewport settings
        bool m_WireframeMode = false;
        bool m_ShowGrid = true;
        bool m_ShowGridAxes = true;
        float m_GridFineSize = 1.0f;
        float m_GridCoarseSize = 10.0f;
        float m_GridLineWidth = 0.02f;
        float m_GridOpacity = 0.4f;
        glm::vec4 m_ColliderColor = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
        bool m_ShowViewportSettings = false;

        Vector<ImGuiPanel*> m_ImGuiWindows;
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

        Mutex m_FileWatchMutex;
        Vector<Path> m_FileWatchQueue;
        bool m_AssemblyReloadPending = false;
        std::chrono::steady_clock::time_point m_LastCsChangeTime;

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
        SceneRenderer* m_SceneRenderer;

    public:
        static EditorCamera& GetEditorCamera() { return s_EditorCamera; }
    };
} // namespace Crowny
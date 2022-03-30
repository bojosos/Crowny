#pragma once

#include "Platform/Vulkan/VulkanRenderAPI.h"

#include "Crowny/ImGui/ImGuiMenu.h"

#include "Panels/AssetBrowserPanel.h"
#include "Panels/ComponentEditor.h"
#include "Panels/ConsolePanel.h"
#include "Panels/GLInfoPanel.h"
#include "Panels/HierarchyPanel.h"
#include "Panels/ImGuiPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ViewportPanel.h"

#include "Crowny/RenderAPI/RenderTarget.h"
#include "Crowny/Renderer/EditorCamera.h"
#include "Crowny/Scene/Scene.h"

#include "Crowny/Common/Time.h"
#include "Crowny/Scripting/Mono/MonoClass.h"

#include <Crowny.h>
#include <entt/entt.hpp>

namespace Crowny
{
    class TextureEditor;
    class ImGuiViewportWindow;

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

        void NewProject();
        void OpenProject();

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

        Vector<ImGuiPanel*> m_ImGuiWindows;
        static EditorCamera s_EditorCamera;
        Entity m_HoveredEntity;
        bool m_CreatingNewProject = false;
        bool m_GameMode = false;
        String m_NewProjectPath;
        String m_NewProjectName;
        glm::vec2 m_ViewportSize = { 1280.0f, 720.0f }; // and dis

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
        static EditorCamera GetEditorCamera() { return s_EditorCamera; }
    };
} // namespace Crowny
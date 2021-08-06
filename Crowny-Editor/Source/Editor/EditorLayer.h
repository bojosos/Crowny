#pragma once

#include "Crowny/ImGui/ImGuiMenu.h"

#include "Panels/ImGuiAssetBrowserPanel.h"
#include "Panels/ImGuiComponentEditor.h"
#include "Panels/ImGuiConsolePanel.h"
#include "Panels/ImGuiGLInfoPanel.h"
#include "Panels/ImGuiHierarchyPanel.h"
#include "Panels/ImGuiInspectorPanel.h"
#include "Panels/ImGuiMaterialPanel.h"
#include "Panels/ImGuiPanel.h"
#include "Panels/ImGuiViewportPanel.h"

#include "Crowny/Renderer/EditorCamera.h"
#include "Crowny/Scene/Scene.h"

#include "Crowny/Common/Time.h"
#include "Crowny/Scripting/CWMonoClass.h"

#include <Crowny.h>
#include <entt/entt.hpp>

namespace Crowny
{
    class ImGuiTextureEditor;
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

        void CreateNewScene();
        void OpenScene();
        void OpenScene(const std::string& filepath);
        void SaveActiveScene();
        void SaveActiveSceneAs();

    private:
        friend class Time;
        ImGuiMenuBar* m_MenuBar;
        Ref<Scene> m_Temp;

        ImGuiPanel* m_GLInfoPanel;
        ImGuiInspectorPanel* m_InspectorPanel;
        ImGuiHierarchyPanel* m_HierarchyPanel;
        ImGuiViewportPanel* m_ViewportPanel;
        ImGuiTextureEditor* m_TextureEditor;
        ImGuiMaterialPanel* m_MaterialEditor;
        ImGuiConsolePanel* m_ConsolePanel;
        ImGuiAssetBrowserPanel* m_AssetBrowser;

        uint32_t m_GizmoMode = 0;

        std::vector<ImGuiPanel*> m_ImGuiWindows;
        static EditorCamera s_EditorCamera;
        Entity m_HoveredEntity;
        bool m_GameMode = false;
        bool m_Paused = false;
        glm::vec2 m_ViewportSize = { 1280.0f, 720.0f }; // and dis

        static float s_DeltaTime;
        static float s_SmoothDeltaTime;
        static float s_RealtimeSinceStartup;
        static float s_Time;
        static float s_FixedDeltaTime;
        static float s_FrameCount;

    public:
        static EditorCamera GetEditorCamera() { return s_EditorCamera; }
    };
} // namespace Crowny
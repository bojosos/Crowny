#include "cwepch.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Scene/SceneManager.h"

#include "Panels/ImGuiComponentEditor.h"
#include "Panels/ImGuiHierarchyPanel.h"
#include "Panels/ImGuiInspectorPanel.h"

#include <imgui.h>

namespace Crowny
{

    ImGuiInspectorPanel::ImGuiInspectorPanel(const std::string& name) : ImGuiPanel(name)
    {
        m_ComponentEditor.RegisterComponent<TransformComponent>("Transform");
        m_ComponentEditor.RegisterComponent<CameraComponent>("Camera");
        m_ComponentEditor.RegisterComponent<MeshRendererComponent>("Mesh Filter");
        m_ComponentEditor.RegisterComponent<TextComponent>("Text");
        m_ComponentEditor.RegisterComponent<SpriteRendererComponent>("Sprite Renderer");
        m_ComponentEditor.RegisterComponent<MonoScriptComponent>("C# Script");

        m_ComponentEditor.RegisterComponent<AudioListenerComponent>("Audio Listener");
        m_ComponentEditor.RegisterComponent<AudioSourceComponent>("Audio Source");
    }

    void ImGuiInspectorPanel::Render()
    {
        ImGui::Begin("Inspector", &m_Shown);
        UpdateState();
        m_ComponentEditor.Render();
        ImGui::End();
    }

    void ImGuiInspectorPanel::Show() { m_Shown = true; }

    void ImGuiInspectorPanel::Hide() { m_Shown = false; }

} // namespace Crowny
#include "cwepch.h"

#include "Editor/EditorLayer.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Events/ImGuiEvent.h"
#include "Crowny/Input/Input.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/Scene/SceneRenderer.h"
#include "Platform/Vulkan/VulkanTexture.h"

#include "Panels/ImGuiHierarchyPanel.h"
#include "Panels/ImGuiViewportPanel.h"

#include <ImGuizmo.h>
#include <backends/imgui_impl_vulkan.h>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

namespace Crowny
{

    ImGuiViewportPanel::ImGuiViewportPanel(const String& name) : ImGuiPanel(name) {}

    void ImGuiViewportPanel::Render()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        ImGui::Begin("Viewport", &m_Shown);
        UpdateState();
        Application::Get().GetImGuiLayer()->BlockEvents(!m_Focused && !m_Hovered);
        if (m_Focused || m_Hovered) // Change gizmo type
        {
            if (Input::IsKeyPressed(Key::W))
                m_GizmoMode = ImGuizmo::TRANSLATE;
            if (Input::IsKeyPressed(Key::E))
                m_GizmoMode = ImGuizmo::ROTATE;
            if (Input::IsKeyPressed(Key::R))
                m_GizmoMode = ImGuizmo::SCALE;
            if (Input::IsKeyPressed(Key::T))
                m_GizmoMode = ImGuizmo::BOUNDS;
        }
        ImVec2 minBound = ImGui::GetWindowPos();
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        ImVec2 viewportOffset = ImGui::GetCursorPos();

        m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };
        RenderTexture* rt = static_cast<RenderTexture*>(m_RenderTarget.get());
        Ref<Texture> texture = rt->GetColorTexture(0);

        ImTextureID textureID = ImGui_ImplVulkan_AddTexture(texture);
        ImGui::Image(textureID, ImVec2(m_ViewportSize.x, m_ViewportSize.y), ImVec2{ 0, 1 },
                     ImVec2{ 1, 0 }); // The viewport itself

        if (ImGui::BeginDragDropTarget()) // Drag drop scenes and meshes
        {
            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_ITEM");
            if (payload)
            {
                const char* path = (const char*)payload->Data;
                ImGuiViewportSceneDraggedEvent event(path);
                OnEvent(event);
            }
            ImGui::EndDragDropTarget();
        }

        ImVec2 windowSize = ImGui::GetWindowSize();
        minBound.x += viewportOffset.x;
        minBound.y += viewportOffset.y;
        ImVec2 maxBound = { minBound.x + windowSize.x, minBound.y + windowSize.y };
        m_ViewportBounds = { minBound.x, minBound.y, maxBound.x, maxBound.y };

        Entity selected = ImGuiHierarchyPanel::GetSelectedEntity();
        EditorCamera camera = EditorLayer::GetEditorCamera();
        const glm::mat4& proj = camera.GetProjection();
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 id(1.0f);
        ImGuizmo::SetRect(m_ViewportBounds.x, m_ViewportBounds.y, windowSize.x, windowSize.y);
        ImGuizmo::DrawGrid(glm::value_ptr(view), glm::value_ptr(proj), glm::value_ptr(id),
                           100.0f); // A 1x1m grid, TODO: depth test

        if (selected && m_GizmoMode != -1)
        {
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();

            float width = (float)ImGui::GetWindowWidth();
            float height = (float)ImGui::GetWindowHeight();
            auto& tc = selected.GetComponent<TransformComponent>();
            glm::mat4 transform = tc.GetTransform();

            bool snap = Input::IsKeyPressed(Key::LeftControl);
            float snapValue = 0.1;
            if (m_GizmoMode == ImGuizmo::OPERATION::ROTATE)
                snapValue = 15.0f;

            float snapValues[3] = { snapValue, snapValue, snapValue };
            ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), (ImGuizmo::OPERATION)m_GizmoMode,
                                 ImGuizmo::LOCAL, glm::value_ptr(transform), nullptr,
                                 snap ? snapValues : nullptr); // TODO: Bounds
            if (ImGuizmo::IsUsing())
            {
                glm::vec3 position, rotation, scale;
                // if (Math::DecomposeMatrix(transform, position, rotation, scale))
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transform),
                                                      glm::value_ptr(position), // This does some wierd things
                                                      glm::value_ptr(rotation), glm::value_ptr(scale));
                glm::vec3 deltaRot = rotation - tc.Rotation;
                tc.Position = position;
                tc.Rotation += deltaRot;
                tc.Scale = scale;
            }
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void ImGuiViewportPanel::SetEventCallback(const EventCallbackFn& onEvent) { OnEvent = onEvent; }

    void ImGuiViewportPanel::SetEditorRenderTarget(const Ref<RenderTarget>& rt) { m_RenderTarget = rt; }

} // namespace Crowny

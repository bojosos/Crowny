#include "cwepch.h"

#include "Editor/EditorLayer.h"
#include "Editor/Editor.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Events/ImGuiEvent.h"
#include "Crowny/Input/Input.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/Scene/SceneRenderer.h"

#include "Panels/HierarchyPanel.h"
#include "UI/UIUtils.h"
#include "Panels/ViewportPanel.h"

#include <ImGuizmo.h>
#include <backends/imgui_impl_vulkan.h>
#include <glm/gtc/type_ptr.hpp>

namespace Crowny
{

    ViewportPanel::ViewportPanel(const String& name) : ImGuiPanel(name), m_ViewportBounds(0.0f)
    {
		Ref<ProjectSettings> projSettings = Editor::Get().GetProjectSettings();
        m_GizmoMode = projSettings->GizmoMode;
	    m_LocalMode = projSettings->GizmoLocalMode;
    }

    void ViewportPanel::Render()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        BeginPanel();
        Application::Get().GetImGuiLayer()->BlockEvents(!m_Focused && !m_Hovered);
        if (m_Focused) // Change gizmo type
        {
			if (Input::IsKeyPressed(Key::Q))
				m_GizmoMode = -1;
            if (Input::IsKeyPressed(Key::W))
                m_GizmoMode = ImGuizmo::TRANSLATE;
            if (Input::IsKeyPressed(Key::E))
                m_GizmoMode = ImGuizmo::ROTATE;
            if (Input::IsKeyPressed(Key::R))
                m_GizmoMode = ImGuizmo::SCALE;
            if (Input::IsKeyPressed(Key::T))
                m_GizmoMode = ImGuizmo::BOUNDS;
			if (Input::IsKeyDown(Key::X))
				m_LocalMode = !m_LocalMode;
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
            if (const ImGuiPayload* payload = UIUtils::AcceptAssetPayload())
            {
                Path assetPath = UIUtils::GetPathFromPayload(payload);
                ImGuiViewportSceneDraggedEvent event(assetPath);
                OnEvent(event);
            }
            ImGui::EndDragDropTarget();
        }

        glm::vec2 bounds[2];
        ImVec2 viewportMinRegion = ImGui::GetWindowContentRegionMin();
        ImVec2 viewportMaxRegion = ImGui::GetWindowContentRegionMax();
        ImVec2 viewportOffset2 = ImGui::GetWindowPos();
        bounds[0] = { viewportMinRegion.x + viewportOffset2.x, viewportMinRegion.y + viewportOffset2.y };
        bounds[1] = { viewportMaxRegion.x + viewportOffset2.x, viewportMaxRegion.y + viewportOffset2.y };
        m_ViewportBounds.x = bounds[0].x;
        m_ViewportBounds.y = bounds[0].y;
        m_ViewportBounds.z = bounds[1].x;
        m_ViewportBounds.w = bounds[1].y;

        Entity selected = HierarchyPanel::GetSelectedEntity();

        EditorCamera& camera = EditorLayer::GetEditorCamera();
        const glm::mat4& proj = camera.GetProjection();
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 id(1.0f);
        ImGuizmo::SetRect(bounds[0].x, bounds[0].y, bounds[1].x - bounds[0].x, bounds[1].y - bounds[0].y);
        // ImGuizmo::DrawGrid(glm::value_ptr(view), glm::value_ptr(proj), glm::value_ptr(id),
        //                 100.0f); // A 1x1m grid, TODO: depth test

        if (selected && m_GizmoMode != -1)
        {
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();

            float width = (float)ImGui::GetWindowWidth();
            float height = (float)ImGui::GetWindowHeight();
            auto& tc = selected.GetComponent<TransformComponent>();
            glm::mat4 transform = tc.GetTransform();

            bool snap = Input::IsKeyPressed(Key::LeftControl);
            float snapValue = 0.1f; // TODO: These snaps should be loaded from the editor settings
            if (m_GizmoMode == ImGuizmo::OPERATION::ROTATE)
                snapValue = 15.0f;

            float snapValues[3] = { snapValue, snapValue, snapValue };
            ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), (ImGuizmo::OPERATION)m_GizmoMode,
                                (!m_LocalMode && m_GizmoMode == ImGuizmo::TRANSLATE) ? ImGuizmo::WORLD : ImGuizmo::LOCAL, glm::value_ptr(transform), nullptr,
                                 snap ? snapValues : nullptr); // TODO: Bounds, does rotation work?
            ImGuizmo::ViewManipulate(glm::value_ptr(view), camera.GetDistance(), {m_ViewportBounds.z - 136.0f, m_ViewportBounds.y}, ImVec2(128, 128), 0x10101010);
			glm::vec3 t, r, s;
			Math::DecomposeMatrix(view, t, r, s);
			// ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(view), glm::value_ptr(t), glm::value_ptr(r), glm::value_ptr(s));
            // r = glm::radians(r);
			// camera.SetPosition(t);
            // camera.SetYaw(r.z);
			// glm::vec3 deltaRot = r - rot;
            // camera.SetPitch(r.x);
			
            if (ImGuizmo::IsUsing())
            {
                glm::vec3 position, rotation, scale;
                if (Math::DecomposeMatrix(transform, position, rotation, scale))
                {
                    glm::vec3 deltaRot = rotation - tc.Rotation;
                    tc.Position = position;
                    tc.Rotation += deltaRot;
                    tc.Scale = scale;
                }
            }
        }

        EndPanel();
        ImGui::PopStyleVar();
    }

    void ViewportPanel::SetEventCallback(const EventCallbackFn& onEvent) { OnEvent = onEvent; }

    void ViewportPanel::SetEditorRenderTarget(const Ref<RenderTarget>& rt) { m_RenderTarget = rt; }

} // namespace Crowny

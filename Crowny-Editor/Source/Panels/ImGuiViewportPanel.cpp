#include "cwepch.h"

#include "Editor/EditorLayer.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Scene/SceneRenderer.h"
#include "Crowny/Input/Input.h"

#include "Panels/ImGuiViewportPanel.h"
#include "Panels/ImGuiHierarchyPanel.h"

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

namespace Crowny
{

	static Ref<EnvironmentMap> envmap;
	ImGuiViewportPanel::ImGuiViewportPanel(const std::string& name) : ImGuiPanel(name)
	{
		
	}

	void ImGuiViewportPanel::Render()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin("Viewport", &m_Shown);
    UpdateState();
		Application::Get().GetImGuiLayer()->BlockEvents(!m_Focused && !m_Hovered);
			
		ImVec2 minBound = ImGui::GetWindowPos();
		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		ImVec2 viewportOffset = ImGui::GetCursorPos();
		m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };
//		uint32_t textureID = SceneRenderer::GetMainFramebuffer()->GetColorAttachmentRendererID();
    uint32_t textureID = 97;
		ImGui::Image(reinterpret_cast<void*>(textureID), ImVec2(m_ViewportSize.x, m_ViewportSize.y), ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
		
		ImVec2 windowSize = ImGui::GetWindowSize();
		minBound.x += viewportOffset.x;
		minBound.y += viewportOffset.y;
		ImVec2 maxBound = { minBound.x + windowSize.x, minBound.y + windowSize.y };
		m_ViewportBounds = { minBound.x, minBound.y, maxBound.x, maxBound.y };
		
		Entity selected = ImGuiHierarchyPanel::GetSelectedEntity();
		if (selected && m_GizmoMode != -1)
		{
			ImGuizmo::SetOrthographic(false);
			ImGuizmo::SetDrawlist();

			float width = (float)ImGui::GetWindowWidth();
			float height = (float)ImGui::GetWindowHeight();
			ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, width, height);
			EditorCamera camera = EditorLayer::GetEditorCamera();
			const glm::mat4& proj = camera.GetProjection();
			glm::mat4 view = camera.GetViewMatrix();
			auto& tc = selected.GetComponent<TransformComponent>();
			glm::mat4 transform = tc.GetTransform();

			bool snap = Input::IsKeyPressed(Key::LeftControl);
			float snapValue = 0.1;
			if (m_GizmoMode == ImGuizmo::OPERATION::ROTATE)
				snapValue = 15.0f;

			float snapValues[3] = { snapValue, snapValue, snapValue };
			ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), (ImGuizmo::OPERATION)m_GizmoMode, ImGuizmo::LOCAL, glm::value_ptr(transform), nullptr, snap ? snapValues : nullptr);
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

		ImGui::End();
		ImGui::PopStyleVar();
	}

}

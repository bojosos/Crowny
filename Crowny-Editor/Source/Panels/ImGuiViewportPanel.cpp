#include "cwepch.h"

#include "Panels/ImGuiViewportPanel.h"
#include "Panels/ImGuiHierarchyPanel.h"
#include "Crowny/Application/Application.h"
#include "Crowny/Input/Input.h"

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

namespace Crowny
{

	static Ref<EnvironmentMap> envmap;
	ImGuiViewportPanel::ImGuiViewportPanel(const std::string& name, const Ref<Framebuffer>& framebuffer, glm::vec2& viewportSize) : ImGuiPanel(name), m_Framebuffer(framebuffer), m_ViewportSize(viewportSize)
	{
		
	}

	void ImGuiViewportPanel::Render()
	{
		bool m_ViewportFocused = ImGui::IsWindowFocused();
		bool m_ViewportHovered = ImGui::IsWindowHovered();
		Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportFocused && !m_ViewportHovered);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin("Viewport", &m_Shown);
			
		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };
		uint64_t textureID = m_Framebuffer->GetColorAttachmentRendererID();
		ImGui::Image(reinterpret_cast<void*>(textureID), ImVec2(m_Framebuffer->GetProperties().Width, m_Framebuffer->GetProperties().Height), ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
		
		Entity selected = ImGuiHierarchyPanel::GetSelectedEntity();
		if (selected && m_GizmoMode != -1)
		{
			ImGuizmo::SetOrthographic(false);
			ImGuizmo::SetDrawlist();

			float width = (float)ImGui::GetWindowWidth();
			float height = (float)ImGui::GetWindowHeight();
			ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, width, height);

			auto cameraEntity = SceneManager::GetActiveScene()->GetCamera();
			const auto& camera = cameraEntity.GetComponent<CameraComponent>().Camera;
			const glm::mat4& proj = camera.GetProjection();
			glm::mat4 view = glm::inverse(cameraEntity.GetComponent<TransformComponent>().GetTransform());
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
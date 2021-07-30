#include "cwepch.h"

#include "Editor/EditorLayer.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Events/ImGuiEvent.h"
#include "Crowny/Input/Input.h"
#include "Crowny/Renderer/RenderTexture.h"
#include "Crowny/Renderer/SamplerState.h"
#include "Crowny/Scene/SceneRenderer.h"
#include "Platform/Vulkan/VulkanSamplerState.h"
#include "Platform/Vulkan/VulkanTexture.h"

#include "Panels/ImGuiViewportPanel.h"
#include "Panels/ImGuiHierarchyPanel.h"

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

namespace Crowny
{
	extern Ref<RenderTarget> renderTarget;
	
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
		RenderTexture* rt = static_cast<RenderTexture*>(renderTarget.get());
		Ref<Texture> texture = rt->GetColorTexture(0);
		
		ImTextureID textureID = ImGui_ImplVulkan_AddTexture(texture);
		ImGui::Image(textureID, ImVec2(m_ViewportSize.x, m_ViewportSize.y), ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
		
		if (ImGui::BeginDragDropTarget())
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

	void ImGuiViewportPanel::SetEventCallback(const EventCallbackFn& onEvent)
	{
		OnEvent = onEvent;
	}

}

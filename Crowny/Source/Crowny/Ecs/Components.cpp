#include "cwpch.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Renderer/Texture.h"
#include "Crowny/Renderer/Camera.h"
#include "Crowny/Common/Color.h"
#include "glm/gtc/type_ptr.hpp"
#include "Crowny/ImGui/ImGuiLayer.h"

namespace Crowny
{
	template <>
	void ComponentEditorWidget<CameraComponent>(entt::registry& reg, entt::entity e)
	{
		auto& t = reg.get<CameraComponent>(e);
		ImGui::ColorEdit3("Background", glm::value_ptr(t.Camera.BackgroundColor));

		auto& cam = t.Camera;

		const char* projections[2] = { "Orthographic", "Perspective" };
		if (ImGui::BeginCombo("Projection", projections[(uint32_t)cam.Projection]))
		{
			for (uint32_t i = 0; i < 2; i++)
			{
				const bool is_selected = ((uint32_t)cam.Projection == i);
				if (ImGui::Selectable(projections[i], is_selected))
					t.Camera.Projection = (CameraProjection)i;

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		if (cam.Projection == CameraProjection::Perspective)
		{
			ImGui::SliderInt("Field of View", &cam.Fov, 0, 180, "%d°");

			ImGui::SetNextItemOpen(true, ImGuiCond_Once);

			if (ImGui::CollapsingHeader("Clipping Planes"))
			{
				ImGui::Indent(30.f);
				static float maxClippingPlane = 100000000;
				static float minClippingPlane = 0.0000001;
				if (ImGui::DragScalar("Near", ImGuiDataType_Float, &cam.ClippingPlanes.x, 0.1f, &minClippingPlane, &maxClippingPlane))
					cam.ClippingPlanes.x = std::clamp(cam.ClippingPlanes.x, minClippingPlane, maxClippingPlane);
				if (ImGui::DragScalar("Far", ImGuiDataType_Float, &cam.ClippingPlanes.y, 0.1f, &minClippingPlane, &maxClippingPlane))
					cam.ClippingPlanes.y = std::clamp(cam.ClippingPlanes.y, minClippingPlane, maxClippingPlane);
				ImGui::Unindent(30.f);
			}

			ImGui::Separator();
			ImGui::SetNextItemOpen(true, ImGuiCond_Once);

			if (ImGui::CollapsingHeader("Viewport Rect"))
			{
				ImGui::Indent(30.f);

				static float minViewport = 0.0f;
				static float maxViewport = 1.0f;
				if (ImGui::DragScalar("X", ImGuiDataType_Float, &cam.ViewportRectangle.x, 0.01f, &minViewport, &maxViewport))
					cam.ViewportRectangle.x = std::clamp(cam.ViewportRectangle.x, minViewport, maxViewport);
				if (ImGui::DragScalar("Y", ImGuiDataType_Float, &cam.ViewportRectangle.z, 0.01f, &minViewport, &maxViewport))
					cam.ViewportRectangle.y = std::clamp(cam.ViewportRectangle.y, minViewport, maxViewport);
				if (ImGui::DragScalar("Width", ImGuiDataType_Float, &cam.ViewportRectangle.y, 0.01f, &minViewport, &maxViewport))
					cam.ViewportRectangle.z = std::clamp(cam.ViewportRectangle.z, minViewport, maxViewport);
				if (ImGui::DragScalar("Height", ImGuiDataType_Float, &cam.ViewportRectangle.w, 0.01f, &minViewport, &maxViewport))
					cam.ViewportRectangle.w = std::clamp(cam.ViewportRectangle.w, minViewport, maxViewport);

				ImGui::Unindent(30.f);
			}

			ImGui::Checkbox("Occlusion Culling", &cam.OcclusionCulling);
			ImGui::Checkbox("HDR", &cam.HDR);
			ImGui::Checkbox("MSAA", &cam.MSAA);
		}
	}

	template <>
	void ComponentEditorWidget<TransformComponent>(entt::registry& reg, entt::entity e)
	{
		auto& t = reg.get<TransformComponent>(e);
		//ImGui::DragFloat("x##Transform", &t.x, 0.1f);
		//ImGui::DragFloat("y##Transform", &t.y, 0.1f);
	}

}
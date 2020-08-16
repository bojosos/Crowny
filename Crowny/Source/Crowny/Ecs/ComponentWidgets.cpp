#include "cwpch.h"

#include "Crowny/Ecs/Components.h"

#include <imgui.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.inl>

namespace Crowny
{

	template <>
	void ComponentEditorWidget<TransformComponent>(Entity& e)
	{
		auto& t = e.GetComponent<TransformComponent>();
		glm::vec3 scale;
		glm::vec3 translation;
		glm::vec3 rotationEuler;
		glm::quat rotation;
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(t.Transform, scale, rotation, translation, skew, perspective);
		rotationEuler = glm::degrees(glm::eulerAngles(rotation));
		bool changed = false;

		changed |= ImGui::DragFloat3("Transform##Transform", glm::value_ptr(translation));
		changed |= ImGui::DragFloat3("Rotation##Rotation", glm::value_ptr(rotationEuler));
		changed |= ImGui::DragFloat3("Scale##Scale", glm::value_ptr(scale));

		if (changed) {
			glm::mat4 tr = glm::translate(glm::mat4(1.0f), translation);
			rotationEuler = glm::radians(rotationEuler);
			//glm::mat4 rot = glm::eulerAngleXYZ(rotationEuler.x, rotationEuler.y, rotationEuler.z);
			tr = glm::rotate(tr, rotationEuler.x, glm::vec3(1.0f, 0.0f, 0.0f));
			tr = glm::rotate(tr, rotationEuler.y, glm::vec3(0.0f, 1.0f, 0.0f));
			tr = glm::rotate(tr, rotationEuler.z, glm::vec3(0.0f, 0.0f, 1.0f));

			t.Transform = glm::scale(tr, scale);
		}
	}

	template <>
	void ComponentEditorWidget<CameraComponent>(Entity& e)
	{
		auto& cam = e.GetComponent<CameraComponent>();
		ImGui::ColorEdit3("Background", glm::value_ptr(cam.BackgroundColor));

		const char* projections[2] = { "Orthographic", "Perspective" };
		if (ImGui::BeginCombo("Projection", projections[(uint32_t)cam.Projection]))
		{
			for (uint32_t i = 0; i < 2; i++)
			{
				const bool is_selected = ((uint32_t)cam.Projection == i);
				if (ImGui::Selectable(projections[i], is_selected))
					cam.Projection = (CameraProjection)i;

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
				static float maxClippingPlane = 1000000.0f;
				static float minClippingPlane = 0.0000001f;
				if (ImGui::DragScalar("Near", ImGuiDataType_Float, &cam.ClippingPlanes.x, 0.1f, &minClippingPlane, &maxClippingPlane, "%.2f"))
					cam.ClippingPlanes.x = std::clamp(cam.ClippingPlanes.x, minClippingPlane, maxClippingPlane);
				if (ImGui::DragScalar("Far", ImGuiDataType_Float, &cam.ClippingPlanes.y, 0.1f, &minClippingPlane, &maxClippingPlane, "%.2f"))
					cam.ClippingPlanes.y = std::clamp(cam.ClippingPlanes.y, minClippingPlane, maxClippingPlane);
				ImGui::Unindent(30.f);
			}

			ImGui::SetNextItemOpen(true, ImGuiCond_Once);

			if (ImGui::CollapsingHeader("Viewport Rect"))
			{
				ImGui::Indent(30.f);

				static float minViewport = 0.0f;
				static float maxViewport = 1.0f;
				if (ImGui::DragScalar("X", ImGuiDataType_Float, &cam.ViewportRectangle.x, 0.01f, &minViewport, &maxViewport, "%.2f"))
					cam.ViewportRectangle.x = std::clamp(cam.ViewportRectangle.x, minViewport, maxViewport);
				if (ImGui::DragScalar("Y", ImGuiDataType_Float, &cam.ViewportRectangle.z, 0.01f, &minViewport, &maxViewport, "%.2f"))
					cam.ViewportRectangle.y = std::clamp(cam.ViewportRectangle.y, minViewport, maxViewport);
				if (ImGui::DragScalar("Width", ImGuiDataType_Float, &cam.ViewportRectangle.y, 0.01f, &minViewport, &maxViewport, "%.2f"))
					cam.ViewportRectangle.z = std::clamp(cam.ViewportRectangle.z, minViewport, maxViewport);
				if (ImGui::DragScalar("Height", ImGuiDataType_Float, &cam.ViewportRectangle.w, 0.01f, &minViewport, &maxViewport, "%.2f"))
					cam.ViewportRectangle.w = std::clamp(cam.ViewportRectangle.w, minViewport, maxViewport);

				ImGui::Unindent(30.f);
			}

			ImGui::Checkbox("Occlusion Culling", &cam.OcclusionCulling);
			ImGui::Checkbox("HDR", &cam.HDR);
			ImGui::Checkbox("MSAA", &cam.MSAA);
		}
	}

	template<>
	void ComponentEditorWidget<SpriteRendererComponent>(Entity& e)
	{
		auto& t = e.GetComponent<SpriteRendererComponent>();

		if (t.Texture) {
			ImGui::Image((ImTextureID)t.Texture->GetRendererID(), { 150.0f, 150.0f });
		}
	}

	template <>
	void ComponentEditorWidget<MeshRendererComponent>(Entity& e) {}

}
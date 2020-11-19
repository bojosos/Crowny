#include "cwpch.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Common/FileSystem.h"

#include "Editor/EditorAssets.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.inl>

namespace Crowny
{

	template <>
	void ComponentEditorWidget<TransformComponent>(Entity& e)
	{
		auto& t = e.GetComponent<TransformComponent>();
		bool changed = false;

		ImGui::Columns(2);
		ImGui::Text("Transform"); ImGui::NextColumn();
		ImGui::DragFloat3("Transform##", glm::value_ptr(t.Position)); ImGui::NextColumn();
		
		ImGui::Text("Rotation"); ImGui::NextColumn();
		glm::vec3 deg = glm::degrees(t.Rotation);
		ImGui::DragFloat3("Rotation##", glm::value_ptr(deg)); ImGui::NextColumn();
		t.Rotation = glm::radians(deg);

		ImGui::Text("Scale"); ImGui::NextColumn();
		ImGui::DragFloat3("Scale##", glm::value_ptr(t.Scale)); ImGui::NextColumn();
		ImGui::Columns(1);
	}

	template <>
	void ComponentEditorWidget<CameraComponent>(Entity& e)
	{
		auto& cam = e.GetComponent<CameraComponent>().Camera;
		ImGui::ColorEdit3("Background", glm::value_ptr(cam.m_BackgroundColor));

		ImGui::Columns(2);
		ImGui::Text("Projection"); ImGui::NextColumn();
		static const char* projections[2] = { "Orthographic", "Perspective" };
		if (ImGui::BeginCombo("Projection##", projections[(uint32_t)cam.m_Projection]))
		{
			for (uint32_t i = 0; i < 2; i++)
			{
				const bool is_selected = ((uint32_t)cam.m_Projection == i);
				if (ImGui::Selectable(projections[i], is_selected))
					cam.m_Projection = (CameraProjection)i;

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::NextColumn();
		if (cam.m_Projection == CameraProjection::Perspective)
		{
			ImGui::Text("Filed of View"); ImGui::NextColumn();
			ImGui::SliderInt("##fov", (int32_t*)&cam.m_Fov, 0, 180, "%d");

			ImGui::Columns(1);
			ImGui::SetNextItemOpen(true, ImGuiCond_Once);

			if (ImGui::CollapsingHeader("Clipping Planes"))
			{
				ImGui::Indent(30.f);
				static float maxClippingPlane = 1000000.0f;
				static float minClippingPlane = 0.0000001f;

				ImGui::Columns(2);
				ImGui::Text("Near"); ImGui::NextColumn();
				if (ImGui::DragScalar("##near", ImGuiDataType_Float, &cam.m_ClippingPlanes.x, 0.1f, &minClippingPlane, &maxClippingPlane, "%.2f"))
					cam.m_ClippingPlanes.x = std::clamp(cam.m_ClippingPlanes.x, minClippingPlane, maxClippingPlane);
				ImGui::NextColumn();
				ImGui::Text("Far"); ImGui::NextColumn();
				if (ImGui::DragScalar("##far", ImGuiDataType_Float, &cam.m_ClippingPlanes.y, 0.1f, &minClippingPlane, &maxClippingPlane, "%.2f"))
					cam.m_ClippingPlanes.y = std::clamp(cam.m_ClippingPlanes.y, minClippingPlane, maxClippingPlane);
				ImGui::Unindent(30.f);
				ImGui::NextColumn();
			}
		}

		else if(cam.m_Projection == CameraProjection::Orthographic)
		{
			 
		}

		ImGui::SetNextItemOpen(true, ImGuiCond_Once);

		ImGui::Columns(1);
		if (ImGui::CollapsingHeader("Viewport Rect"))
		{
			ImGui::Columns(2);
			ImGui::Indent(30.f);

			static float minViewport = 0.0f;
			static float maxViewport = 1.0f;

			ImGui::Text("X"); ImGui::NextColumn();
			if (ImGui::DragScalar("##rectx", ImGuiDataType_Float, &cam.m_ViewportRectangle.x, 0.01f, &minViewport, &maxViewport, "%.2f"))
				cam.m_ViewportRectangle.x = std::clamp(cam.m_ViewportRectangle.x, minViewport, maxViewport);
			ImGui::NextColumn();

			ImGui::Text("Y"); ImGui::NextColumn();
			if (ImGui::DragScalar("##recty", ImGuiDataType_Float, &cam.m_ViewportRectangle.y, 0.01f, &minViewport, &maxViewport, "%.2f"))
				cam.m_ViewportRectangle.y = std::clamp(cam.m_ViewportRectangle.y, minViewport, maxViewport);
			ImGui::NextColumn();

			ImGui::Text("Width"); ImGui::NextColumn();
			if (ImGui::DragScalar("##rectw", ImGuiDataType_Float, &cam.m_ViewportRectangle.z, 0.01f, &minViewport, &maxViewport, "%.2f"))
				cam.m_ViewportRectangle.z = std::clamp(cam.m_ViewportRectangle.z, minViewport, maxViewport);
			ImGui::NextColumn();

			ImGui::Text("Height"); ImGui::NextColumn();
			if (ImGui::DragScalar("##recth", ImGuiDataType_Float, &cam.m_ViewportRectangle.w, 0.01f, &minViewport, &maxViewport, "%.2f"))
				cam.m_ViewportRectangle.w = std::clamp(cam.m_ViewportRectangle.w, minViewport, maxViewport);
			ImGui::Columns(1);
			ImGui::Unindent(30.f);
		}

		ImGui::Columns(2);
		ImGui::Text("Occlusion Culling"); ImGui::NextColumn();
		ImGui::Checkbox("##occcull", &cam.m_OcclusionCulling); ImGui::NextColumn();
		ImGui::Text("HDR"); ImGui::NextColumn();
		ImGui::Checkbox("##hdr", &cam.m_HDR); ImGui::NextColumn();
		ImGui::Text("MSAA"); ImGui::NextColumn();
		ImGui::Checkbox("##msaa", &cam.m_MSAA);
		ImGui::Columns(1);
	}

	int OnTextEdited(ImGuiInputTextCallbackData* data)
	{
		CW_ENGINE_INFO(data->Buf);
		return 0;
	}

	template<>
	void ComponentEditorWidget<TextComponent>(Entity& e)
	{
		auto& t = e.GetComponent<TextComponent>();

		ImGui::Columns(2);
		ImGui::Text("Text"); ImGui::NextColumn();
		ImGui::InputText("##text", &t.Text);  ImGui::NextColumn();

		ImGui::Text("Color"); ImGui::NextColumn();
		ImGui::ColorEdit4("##textcolor", glm::value_ptr(t.Color));  ImGui::NextColumn();

		ImGui::Text("Font"); ImGui::NextColumn();
		ImGui::Text(t.Font->GetName().c_str()); 
#ifdef CW_DEBUG
		ImGui::SameLine();
		if (ImGui::Button("Show Font Atlas"))
		{
			ImGui::OpenPopup(t.Font->GetName().c_str());	
		}

		if (ImGui::BeginPopup(t.Font->GetName().c_str()))
		{
			ImGui::Text(t.Font->GetName().c_str());
			ImGui::Separator();
			ImGui::Image(reinterpret_cast<void*>(t.Font->GetTexture()->GetRendererID()), ImVec2(t.Font->GetTexture()->GetWidth(), t.Font->GetTexture()->GetHeight()));
			ImGui::EndPopup();
		}
#endif
		ImGui::NextColumn();
		
		ImGui::Text("Font Size"); ImGui::NextColumn();

		int32_t size = t.Font->GetSize();
		if (ImGui::InputInt("##fontsize", &size))
		{
			t.Font = FontManager::Get(t.Font->GetName(), size);
		}
		ImGui::Columns(1);
	}

	template<>
	void ComponentEditorWidget<SpriteRendererComponent>(Entity& e)
	{
		auto& t = e.GetComponent<SpriteRendererComponent>();

		if(t.Texture)
			ImGui::Image(reinterpret_cast<void*>(t.Texture->GetRendererID()), { 150.0f, 150.0f });
		else
			ImGui::Image(reinterpret_cast<void*>(EditorAssets::UnassignedTexture->GetRendererID()), { 150.0f, 150.0f });

		if (ImGui::IsItemClicked())
		{
			auto [res, path] = FileSystem::OpenFileDialog("Image files\0*.jpg;*.png\0", "", "Open Image");
			if (res)
			{
				t.Texture = Texture2D::Create(path);
			}
		}

		ImGui::ColorEdit4("Color", glm::value_ptr(t.Color));
	}

	template <>
	void ComponentEditorWidget<MeshRendererComponent>(Entity& e) 
	{
		auto& mesh = e.GetComponent<MeshRendererComponent>().Mesh;

		ImGui::Text("Path");
	}

	template <>
	void ComponentEditorWidget<MonoScriptComponent>(Entity& e)
	{
		auto& script = e.GetComponent<MonoScriptComponent>();
		ImGui::Columns(2);
		ImGui::Text("Script"); ImGui::NextColumn();

		if (!script.Class)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(255, 0, 0));
		}
		else
			ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(0, 255, 0));

		if (ImGui::InputText("##scriptName", &script.Name))
		{
			script.Class = CWMonoRuntime::GetClientAssembly()->GetClass("Sandbox", script.Name);
		}
		
		ImGui::PopStyleColor(1);

		if (!script.Class)
		{
			ImGui::Columns(1);
			return;
		}

		ImGui::NextColumn();

		auto fields = script.Class->GetFields();
		for (auto* field : fields)
		{
			if (field)
			{
				ImGui::Text(field->GetName().c_str()); ImGui::NextColumn();
			}
		}
		ImGui::Columns(1);
	}

}

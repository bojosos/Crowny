#include "cwpch.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Common/FileSystem.h"

#include "Editor/EditorAssets.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <mono/metadata/object.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.inl>

namespace Crowny
{

	template <>
	void ComponentEditorWidget<TransformComponent>(Entity e)
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
	void ComponentEditorWidget<CameraComponent>(Entity e)
	{
		auto& cam = e.GetComponent<CameraComponent>().Camera;
		glm::vec3 tmp = cam.GetBackgroundColor();
		if (ImGui::ColorEdit3("Background", glm::value_ptr(tmp)))
			cam.SetBackgroundColor(tmp);

		ImGui::Columns(2);
		ImGui::Text("Projection"); ImGui::NextColumn();
		const char* projections[2] = { "Orthographic", "Perspective" };
		if (ImGui::BeginCombo("Projection##", projections[(int32_t)cam.GetProjectionType()]))
		{
			for (uint32_t i = 0; i < 2; i++)
			{
				const bool is_selected = ((uint32_t)cam.GetProjectionType() == i);
				if (ImGui::Selectable(projections[i], is_selected))
				{
					cam.SetProjectionType((SceneCamera::CameraProjection)i);
				}

				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::NextColumn();
		if (cam.GetProjectionType() == SceneCamera::CameraProjection::Perspective)
		{
			ImGui::Text("Filed of View"); ImGui::NextColumn();
			float fov = glm::degrees(cam.GetPerspectiveVerticalFOV());
			if (ImGui::SliderFloat("##fov", &fov, 0, 180, "%.3f"))
				cam.SetPerspectiveVerticalFOV(glm::radians(fov));

			ImGui::Columns(1);
			ImGui::SetNextItemOpen(true, ImGuiCond_Once);

			if (ImGui::CollapsingHeader("Clipping Planes"))
			{
				ImGui::Indent(30.f);
				static float maxClippingPlane = 1000000.0f;
				static float minClippingPlane = 0.0000001f;

				ImGui::Columns(2);
				ImGui::Text("Near"); ImGui::NextColumn();
				float near = cam.GetPerspectiveNearClip();
				if (ImGui::DragScalar("##near", ImGuiDataType_Float, &near, 0.1f, &minClippingPlane, &maxClippingPlane, "%.2f"))
					cam.SetPerspectiveNearClip(std::clamp(near, minClippingPlane, maxClippingPlane));
				ImGui::NextColumn();

				float far = cam.GetPerspectiveFarClip();
				ImGui::Text("Far"); ImGui::NextColumn();
				if (ImGui::DragScalar("##far", ImGuiDataType_Float, &far, 0.1f, &minClippingPlane, &maxClippingPlane, "%.2f"))
					cam.SetPerspectiveFarClip(std::clamp(far, minClippingPlane, maxClippingPlane));
				ImGui::Unindent(30.f);
				ImGui::NextColumn();
			}
		}

		else if(cam.GetProjectionType() == SceneCamera::CameraProjection::Orthographic)
		{
			ImGui::Text("Size"); ImGui::NextColumn();
			float size = cam.GetOrthographicSize();
			if (ImGui::SliderFloat("##fov", &size, 0.0f, 180.0f, "%.3f"))
				cam.SetOrthographicSize(size);

			ImGui::Columns(1);
			ImGui::SetNextItemOpen(true, ImGuiCond_Once);

			if (ImGui::CollapsingHeader("Clipping Planes"))
			{
				ImGui::Indent(30.f);
				static float maxClippingPlane = 1000000.0f;
				static float minClippingPlane = 0.0000001f;

				ImGui::Columns(2);
				ImGui::Text("Near"); ImGui::NextColumn();
				float near = cam.GetOrthographicNearClip();
				if (ImGui::DragScalar("##near", ImGuiDataType_Float, &near, 0.1f, &minClippingPlane, &maxClippingPlane, "%.2f"))
					cam.SetOrthographicNearClip(std::clamp(near, minClippingPlane, maxClippingPlane));
				ImGui::NextColumn();

				float far = cam.GetOrthographicFarClip();
				ImGui::Text("Far"); ImGui::NextColumn();
				if (ImGui::DragScalar("##far", ImGuiDataType_Float, &far, 0.1f, &minClippingPlane, &maxClippingPlane, "%.2f"))
					cam.SetOrthographicFarClip(std::clamp(far, minClippingPlane, maxClippingPlane));
				ImGui::Unindent(30.f);
				ImGui::NextColumn();
			}
		}

		ImGui::SetNextItemOpen(true, ImGuiCond_Once);

		ImGui::Columns(1);
		if (ImGui::CollapsingHeader("Viewport Rect"))
		{
			ImGui::Columns(2);
			ImGui::Indent(30.f);

			const float minViewport = 0.0f;
			const float maxViewport = 1.0f;

			ImGui::Text("X"); ImGui::NextColumn();
			glm::vec4 tmp = cam.GetViewportRect();
			ImGui::DragScalar("##rectx", ImGuiDataType_Float, &tmp.x, 0.01f, &minViewport, &maxViewport, "%.2f");
			ImGui::NextColumn();

			ImGui::Text("Y"); ImGui::NextColumn();
			ImGui::DragScalar("##recty", ImGuiDataType_Float, &tmp.y, 0.01f, &minViewport, &maxViewport, "%.2f");
			ImGui::NextColumn();

			ImGui::Text("Width"); ImGui::NextColumn();
			ImGui::DragScalar("##rectw", ImGuiDataType_Float, &tmp.z, 0.01f, &minViewport, &maxViewport, "%.2f");
			ImGui::NextColumn();

			ImGui::Text("Height"); ImGui::NextColumn();
			ImGui::DragScalar("##recth", ImGuiDataType_Float, &tmp.w, 0.01f, &minViewport, &maxViewport, "%.2f");

			tmp.x = std::clamp(tmp.x, minViewport, maxViewport);
			tmp.y = std::clamp(tmp.y, minViewport, maxViewport);
			tmp.z = std::clamp(tmp.z, minViewport, maxViewport);
			tmp.w = std::clamp(tmp.w, minViewport, maxViewport);
			cam.SetViewportRect(tmp);

			ImGui::Columns(1);
			ImGui::Unindent(30.f);
		}

		ImGui::Columns(2);
		ImGui::Text("Occlusion Culling"); ImGui::NextColumn();
		bool occ = cam.GetOcclusionCulling();
		if (ImGui::Checkbox("##occcull", &occ))
			cam.SetOcclusionCulling(occ);
		ImGui::NextColumn();

		ImGui::Text("HDR"); ImGui::NextColumn();
		bool hdr = cam.GetHDR();
		if (ImGui::Checkbox("##hdr", &hdr))
			cam.SetHDR(hdr);
		 ImGui::NextColumn();

		ImGui::Text("MSAA"); ImGui::NextColumn();
		bool msaa = cam.GetMSAA();
		if (ImGui::Checkbox("##msaa", &msaa))
			cam.SetMSAA(msaa);
		ImGui::Columns(1);
	}

	template<>
	void ComponentEditorWidget<TextComponent>(Entity e)
	{
		auto& t = e.GetComponent<TextComponent>();

		ImGui::Columns(2);
		ImGui::Text("Text"); ImGui::NextColumn();
		ImGui::InputText("##text", &t.Text);  ImGui::NextColumn();

		ImGui::Text("Color"); ImGui::NextColumn();
		ImGui::ColorEdit4("##textcolor", glm::value_ptr(t.Color));  ImGui::NextColumn();

		ImGui::Text("Font"); ImGui::NextColumn();
		ImGui::Text("%s", t.Font->GetName().c_str()); 
#ifdef CW_DEBUG
		ImGui::SameLine();
		if (ImGui::Button("Show Font Atlas"))
		{
			ImGui::OpenPopup(t.Font->GetName().c_str());	
		}

		if (ImGui::BeginPopup(t.Font->GetName().c_str()))
		{
			ImGui::Text("%s", t.Font->GetName().c_str());
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
	void ComponentEditorWidget<SpriteRendererComponent>(Entity e)
	{
		auto& t = e.GetComponent<SpriteRendererComponent>();

		if(t.Texture)
			ImGui::Image(reinterpret_cast<void*>(t.Texture->GetRendererID()), { 150.0f, 150.0f });
		else
			ImGui::Image(reinterpret_cast<void*>(EditorAssets::Get().UnassignedTexture->GetRendererID()), { 150.0f, 150.0f });

		if (ImGui::IsItemClicked())
		{
			std::vector<std::string> outPaths;
			if (FileSystem::OpenFileDialog(FileDialogType::OpenFile, "", "", outPaths))
			{
				t.Texture = Texture2D::Create(outPaths[0]);
			}
		}

		ImGui::ColorEdit4("Color", glm::value_ptr(t.Color));
	}

	template <>
	void ComponentEditorWidget<MeshRendererComponent>(Entity e) 
	{
		auto& mesh = e.GetComponent<MeshRendererComponent>().Mesh;

		ImGui::Text("Path");
	}

	template <>
	void ComponentEditorWidget<MonoScriptComponent>(Entity e)
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
		std::string name = script.Class ? std::string(script.Class->GetName()) : "";
		if (ImGui::InputText("##scriptName", &name))
		{
			script.Class = CWMonoRuntime::GetClientAssembly()->GetClass("Sandbox", name);
		}
		
		ImGui::PopStyleColor(1);

		if (!script.Class)
		{
			ImGui::Columns(1);
			return;
		}

		ImGui::NextColumn();
		for (uint32_t i = 0; i < script.DisplayableFields.size(); i++)
		{
			auto* field = script.DisplayableFields[i];
			ImGui::PushID(i);
			ImGui::Text("%s", field->GetName().c_str()); ImGui::NextColumn();
			if (script.ManagedInstance)
			{
				switch (field->GetPrimitiveType())
				{
					case (MonoPrimitiveType::Bool):
					{
						bool value = false;
						field->Get(script.ManagedInstance, &value);
						if (ImGui::Checkbox("##bool", &value))
							field->Set(script.ManagedInstance, &value);
						break;
					}
					case (MonoPrimitiveType::String):
					{
						MonoString* value;
						field->Get(script.ManagedInstance, value);
						std::string nativeValue = MonoUtils::FromMonoString(value);
						if (ImGui::InputText("##field2", &nativeValue))
							field->Set(script.ManagedInstance, MonoUtils::ToMonoString(nativeValue));
						break;
					}
					case (MonoPrimitiveType::ValueType):
					{
						if (MonoUtils::IsEnum(field->GetType()->GetInternalPtr()))
						{
              // TODO: These commonly used methods (GetEnumNames, Compile) should be stored globally instead of having to get them every time
              // Also no mono code in the editor please.
							void* enumType = (void*)mono_type_get_object(CWMonoRuntime::GetDomain(), MonoUtils::GetType(field->GetType()->GetInternalPtr()));
							MonoArray* ar = (MonoArray*)CWMonoRuntime::GetBuiltinClasses().ScriptUtils->GetMethod("GetEnumNames", 1)->Invoke(nullptr, &enumType);
							uint32_t size = mono_array_length(ar);
							std::vector<std::string> enumValues;
							enumValues.resize(size);
							for (uint32_t i = 0; i < size; i++) // Do this once!
							{
								enumValues[i] = MonoUtils::FromMonoString(mono_array_get(ar, MonoString*, i));
							}
							uint32_t value;
							field->Get(script.ManagedInstance, &value);
							if (ImGui::BeginCombo("##enum", enumValues[value].c_str()))
							{
								uint32_t newSelected = value;
								for (uint32_t i = 0; i < size; i++)
								{
									if (ImGui::Selectable(enumValues[i].c_str(), i == value))
									{
										newSelected = i;
									}
								}
								if (newSelected != value)
								{
									void* tmp = &newSelected;
									field->Set(script.ManagedInstance, tmp);
								}
								ImGui::EndCombo();
							}
						}
						break;
					}
					default:
					{		
						CWMonoClass* rng = CWMonoRuntime::GetBuiltinClasses().RangeAttribute;
						MonoObject* obj = field->GetAttribute(rng);
						if(obj)
						{
							float min, max, val;
							rng->GetField("min")->Get(obj, &min); // TODO: cache these
							rng->GetField("max")->Get(obj, &max);
							field->Get(script.ManagedInstance, &val);
							if (ImGui::SliderFloat("##slider", &val, min, max))
								field->Set(script.ManagedInstance, &val); // These should prob be set after compilation, not here
						}
						break;
					}
				}
			}

			ImGui::PopID();
		}
		
		for (CWMonoProperty* prop : script.DisplayableProperties)
		{
			ImGui::Text("%s", prop->GetName().c_str()); ImGui::NextColumn();
			if (script.ManagedInstance) // Might not need it.
			{
				CWMonoClass* rng = CWMonoRuntime::GetBuiltinClasses().RangeAttribute;
				MonoObject* obj = prop->GetAttribute(rng);
				if(obj)
				{
					float min, max, val;
					rng->GetField("min")->Get(obj, &min);
					rng->GetField("max")->Get(obj, &max);
					//val = *prop->Get(script.Instance); // have to box
					//if (ImGui::SliderFloat("##field1", &val, min, max))
					//	prop->Set(script.Instance, &val);
				}
			}
		}
		
		ImGui::Columns(1);
	}

}

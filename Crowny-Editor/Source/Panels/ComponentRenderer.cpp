#include "cwepch.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Ecs/Components.h"

#include "Editor/EditorAssets.h"
#include "Editor/EditorDefaults.h"
#include "Panels/UIUtils.h"

#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include "glm/fwd.hpp"

namespace Crowny
{

	template <> void ComponentEditorWidget<TransformComponent>(Entity e)
	{
		auto& t = e.GetComponent<TransformComponent>();
		bool changed = false;

		ImGui::Columns(2);
		ImGui::Text("Transform");
		ImGui::NextColumn();
		ImGui::DragFloat3("##TransformTransform", glm::value_ptr(t.Position), DRAG_SENSITIVITY);
		ImGui::NextColumn();

		ImGui::Text("Rotation");
		ImGui::NextColumn();
		glm::vec3 deg = glm::degrees(t.Rotation);
		ImGui::DragFloat3("##TransformRotation", glm::value_ptr(deg), DRAG_SENSITIVITY);
		ImGui::NextColumn();
		t.Rotation = glm::radians(deg);

		ImGui::Text("Scale");
		ImGui::NextColumn();
		ImGui::DragFloat3("##TransformScale", glm::value_ptr(t.Scale), DRAG_SENSITIVITY);
		ImGui::NextColumn();
		ImGui::Columns(1);
	}

	template <> void ComponentEditorWidget<CameraComponent>(Entity e)
	{
		auto& cam = e.GetComponent<CameraComponent>().Camera;
		glm::vec3 tmp = cam.GetBackgroundColor();
		if (ImGui::ColorEdit3("Background", glm::value_ptr(tmp)))
			cam.SetBackgroundColor(tmp);

		ImGui::Columns(2);
		ImGui::Text("Projection");
		ImGui::NextColumn();
		const char* projections[2] = { "Orthographic", "Perspective" };
		if (ImGui::BeginCombo("##Projection", projections[(int32_t)cam.GetProjectionType()]))
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
			ImGui::Text("Filed of View");
			ImGui::NextColumn();
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
				ImGui::Text("Near");
				ImGui::NextColumn();
				float nearPlane = cam.GetPerspectiveNearClip();
				if (ImGui::DragScalar("##near", ImGuiDataType_Float, &nearPlane, 0.1f, &minClippingPlane, &maxClippingPlane,
					"%.2f"))
					cam.SetPerspectiveNearClip(std::clamp(nearPlane, minClippingPlane, maxClippingPlane));
				ImGui::NextColumn();

				float farPlane = cam.GetPerspectiveFarClip();
				ImGui::Text("Far");
				ImGui::NextColumn();
				if (ImGui::DragScalar("##far", ImGuiDataType_Float, &farPlane, 0.1f, &minClippingPlane, &maxClippingPlane,
					"%.2f"))
					cam.SetPerspectiveFarClip(std::clamp(farPlane, minClippingPlane, maxClippingPlane));
				ImGui::Unindent(30.f);
				ImGui::NextColumn();
			}
		}

		else if (cam.GetProjectionType() == SceneCamera::CameraProjection::Orthographic)
		{
			ImGui::Text("Size");
			ImGui::NextColumn();
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
				ImGui::Text("Near");
				ImGui::NextColumn();
				float nearPlane = cam.GetOrthographicNearClip();
				if (ImGui::DragScalar("##near", ImGuiDataType_Float, &nearPlane, 0.1f, &minClippingPlane, &maxClippingPlane,
					"%.2f"))
					cam.SetOrthographicNearClip(std::clamp(nearPlane, minClippingPlane, maxClippingPlane));
				ImGui::NextColumn();

				float farPlane = cam.GetOrthographicFarClip();
				ImGui::Text("Far");
				ImGui::NextColumn();
				if (ImGui::DragScalar("##far", ImGuiDataType_Float, &farPlane, 0.1f, &minClippingPlane, &maxClippingPlane,
					"%.2f"))
					cam.SetOrthographicFarClip(std::clamp(farPlane, minClippingPlane, maxClippingPlane));
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

			ImGui::Text("X");
			ImGui::NextColumn();
			glm::vec4 tmp = cam.GetViewportRect();
			ImGui::DragScalar("##rectx", ImGuiDataType_Float, &tmp.x, 0.01f, &minViewport, &maxViewport, "%.2f");
			ImGui::NextColumn();

			ImGui::Text("Y");
			ImGui::NextColumn();
			ImGui::DragScalar("##recty", ImGuiDataType_Float, &tmp.y, 0.01f, &minViewport, &maxViewport, "%.2f");
			ImGui::NextColumn();

			ImGui::Text("Width");
			ImGui::NextColumn();
			ImGui::DragScalar("##rectw", ImGuiDataType_Float, &tmp.z, 0.01f, &minViewport, &maxViewport, "%.2f");
			ImGui::NextColumn();

			ImGui::Text("Height");
			ImGui::NextColumn();
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
		ImGui::Text("Occlusion Culling");
		ImGui::NextColumn();
		bool occ = cam.GetOcclusionCulling();
		if (ImGui::Checkbox("##occcull", &occ))
			cam.SetOcclusionCulling(occ);
		ImGui::NextColumn();

		ImGui::Text("HDR");
		ImGui::NextColumn();
		bool hdr = cam.GetHDR();
		if (ImGui::Checkbox("##hdr", &hdr))
			cam.SetHDR(hdr);
		ImGui::NextColumn();

		ImGui::Text("MSAA");
		ImGui::NextColumn();
		bool msaa = cam.GetMSAA();
		if (ImGui::Checkbox("##msaa", &msaa))
			cam.SetMSAA(msaa);
		ImGui::Columns(1);
	}

	template <> void ComponentEditorWidget<TextComponent>(Entity e)
	{
		auto& t = e.GetComponent<TextComponent>();

		ImGui::Columns(2);
		ImGui::Text("Text");
		ImGui::NextColumn();
		ImGui::InputText("##text", &t.Text);
		ImGui::NextColumn();

		ImGui::Text("Color");
		ImGui::NextColumn();
		ImGui::ColorEdit4("##textcolor", glm::value_ptr(t.Color));
		ImGui::NextColumn();

		ImGui::Text("Font"); // Hook up drag drop here.
		ImGui::NextColumn();
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
			ImGui::Image(ImGui_ImplVulkan_AddTexture(t.Font->GetTexture()),
				{ (float)t.Font->GetTexture()->GetWidth(), (float)t.Font->GetTexture()->GetHeight() });
			ImGui::EndPopup();
		}
#endif
		ImGui::NextColumn();

		ImGui::Text("Font Size");
		ImGui::NextColumn();

		float size = t.Font->GetSize();
		if (ImGui::InputFloat("##fontsize", &size))
			t.Font = FontManager::Get(t.Font->GetName(), size);
		ImGui::Columns(1);
	}

	template <> void ComponentEditorWidget<SpriteRendererComponent>(Entity e)
	{
		auto& t = e.GetComponent<SpriteRendererComponent>();

		if (t.Texture)
			ImGui::Image(ImGui_ImplVulkan_AddTexture(t.Texture), { 50.0f, 50.0f }, { 0, 1 }, { 1, 0 });
		else
			ImGui::Image(ImGui_ImplVulkan_AddTexture(EditorAssets::Get().UnassignedTexture), { 50.0f, 50.0f }, { 0, 1 },
				{ 1, 0 });
		if (ImGui::IsItemClicked())
		{
			Vector<Path> outPaths;
			if (FileSystem::OpenFileDialog(FileDialogType::OpenFile, "", { }, outPaths))
			{
				Ref<Texture> result;
				// LoadTexture(outPaths[0], result);
				t.Texture = result;
			}
		}

		ImGui::SameLine();
		ImGui::ColorEdit4("##SpriteColor", glm::value_ptr(t.Color));
	}

	template <> void ComponentEditorWidget<MeshRendererComponent>(Entity e)
	{
		auto& mesh = e.GetComponent<MeshRendererComponent>().Mesh;

		ImGui::Text("Path");
	}

	template <> void ComponentEditorWidget<Rigidbody2DComponent>(Entity e)
	{
		Rigidbody2DComponent& rb2d = e.GetComponent<Rigidbody2DComponent>();

		ImGui::Columns(2);
		ImGui::Text("Body Type");
		ImGui::NextColumn();

		const char* bodyTypes[3] = { "Static", "Dynamic", "Kinematic" };
		if (ImGui::BeginCombo("##rb2dbodyType", bodyTypes[(uint32_t)rb2d.GetBodyType()]))
		{
			for (uint32_t i = 0; i < 3; i++)
			{
				const bool isSelected = ((uint32_t)rb2d.GetBodyType() == i);
				if (ImGui::Selectable(bodyTypes[i], isSelected))
					rb2d.SetBodyType((BodyType)i);

				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::NextColumn();
		ImGui::Text("Mass"); ImGui::NextColumn();
		float mass = rb2d.GetMass();
		if (ImGui::DragFloat("##rb2dmass", &mass, DRAG_SENSITIVITY))
			rb2d.SetMass(mass);
		ImGui::NextColumn();

		ImGui::NextColumn();
		ImGui::Text("Gravity Scale"); ImGui::NextColumn();
		float gravityScale = rb2d.GetGravityScale();
		if (ImGui::DragFloat("##rb2dmass", &gravityScale, DRAG_SENSITIVITY))
			rb2d.SetGravityScale(gravityScale);

		ImGui::Columns(1);
		ImGui::SetNextItemOpen(true, ImGuiCond_Once);

		if (ImGui::CollapsingHeader("Constraints"))
		{
			ImGui::Indent(30.f);
			ImGui::Columns(2);
			ImGui::Text("Fixed Position");
			ImGui::NextColumn();

			ImGui::Text("X");
			ImGui::SameLine();
			Rigidbody2DConstraints constraints = rb2d.GetConstraints();
			bool freezeX = constraints.IsSet(Rigidbody2DConstraintsBits::FreezePositionX);
			if (ImGui::Checkbox("##rb2dxPosLock", &freezeX))
			{
				if (freezeX)
					constraints.Set(Rigidbody2DConstraintsBits::FreezePositionX);
				else
					constraints.Unset(Rigidbody2DConstraintsBits::FreezePositionX);
				rb2d.SetConstraints(constraints);
			}

			ImGui::SameLine();
			ImGui::Text("Y");
			ImGui::SameLine();
			bool freezeY = rb2d.GetConstraints().IsSet(Rigidbody2DConstraintsBits::FreezePositionY);
			if (ImGui::Checkbox("##rb2dyPosLock", &freezeY))
			{
				if (freezeY)
					constraints.Set(Rigidbody2DConstraintsBits::FreezePositionY);
				else
					constraints.Unset(Rigidbody2DConstraintsBits::FreezePositionY);
				rb2d.SetConstraints(constraints);
			}
			ImGui::NextColumn();
			ImGui::Text("Fixed Rotation");
			ImGui::NextColumn();
			ImGui::Text("Z");
			ImGui::SameLine();
			bool freezeRotation = rb2d.GetConstraints().IsSet(Rigidbody2DConstraintsBits::FreezeRotation);
			if (ImGui::Checkbox("##rb2dyRotLock", &freezeRotation))
			{
				if (freezeRotation)
					constraints.Set(Rigidbody2DConstraintsBits::FreezeRotation);
				else
					constraints.Unset(Rigidbody2DConstraintsBits::FreezeRotation);
				rb2d.SetConstraints(constraints);
			}

			ImGui::Unindent(30.0f);
		}

		ImGui::Columns(1);
	}

	static void DrawPhysicsMaterial(PhysicsMaterial2D& material)
	{
		ImGui::Columns(2);
		ImGui::Text("Density");
		ImGui::NextColumn();
		ImGui::DragFloat("##physicsMatDensity", &material.Density, DRAG_SENSITIVITY);
		ImGui::NextColumn();
		ImGui::Text("Friction");
		ImGui::NextColumn();
		ImGui::DragFloat("##physicsMatFriction", &material.Friction, DRAG_SENSITIVITY);
		ImGui::NextColumn();
		ImGui::Text("Restitution");
		ImGui::NextColumn();
		ImGui::DragFloat("##physicsMatRestition", &material.Restitution, DRAG_SENSITIVITY);
		ImGui::NextColumn();
		ImGui::Text("Restitution Threshold");
		ImGui::NextColumn();
		ImGui::DragFloat("##physicsMatRestitionThreshold", &material.RestitutionThreshold, DRAG_SENSITIVITY);
		ImGui::NextColumn();
	}

	template <> void ComponentEditorWidget<BoxCollider2DComponent>(Entity e)
	{
		BoxCollider2DComponent& bc2d = e.GetComponent<BoxCollider2DComponent>();

		ImGui::Columns(2);
		ImGui::Text("Offset");
		ImGui::NextColumn();
		ImGui::DragFloat2("##boxCollider2Doffset", glm::value_ptr(bc2d.Offset), DRAG_SENSITIVITY);
		ImGui::NextColumn();

		ImGui::Text("Size");
		ImGui::NextColumn();
		ImGui::DragFloat2("##boxCollider2Dsize", glm::value_ptr(bc2d.Size), DRAG_SENSITIVITY);
		ImGui::NextColumn();

		ImGui::Text("Is Trigger");
		ImGui::NextColumn();
		ImGui::Checkbox("##boxCollider2Dtrigger", &bc2d.IsTrigger);
		ImGui::NextColumn();
		DrawPhysicsMaterial(bc2d.Material);
		ImGui::Columns(1);
	}

	template <> void ComponentEditorWidget<CircleCollider2DComponent>(Entity e)
	{
		auto& cc2d = e.GetComponent<CircleCollider2DComponent>();

		ImGui::Columns(2);
		ImGui::Text("Offset");
		ImGui::NextColumn();
		ImGui::DragFloat2("##circleCollider2Doffset", glm::value_ptr(cc2d.Offset), DRAG_SENSITIVITY);
		ImGui::NextColumn();

		ImGui::Text("Radius");
		ImGui::NextColumn();
		ImGui::DragFloat("##circleCollider2Dradius", &cc2d.Radius, DRAG_SENSITIVITY);
		ImGui::NextColumn();

		// ImGui::Text("Is Trigger"); ImGui::NextColumn();
		// ImGui::Checkbox("##circleCollider2Dtrigger", &cc2d.IsTrigger);
		// ImGui::NextColumn();
		DrawPhysicsMaterial(cc2d.Material);

		ImGui::Columns(1);
	}

	template <> void ComponentEditorWidget<AudioListenerComponent>(Entity e) {}

	template <> void ComponentEditorWidget<AudioSourceComponent>(Entity e)
	{
		AudioSourceComponent& sourceComponent = e.GetComponent<AudioSourceComponent>();

		ImGui::Columns(2);

		ImGui::Text("Audio Clip");
		ImGui::NextColumn();
		if (sourceComponent.GetClip() != nullptr)
			ImGui::Text("%s", sourceComponent.GetClip()->GetName().c_str());
		else
			ImGui::Text("Audio clip goes here");
		ImGui::NextColumn();

		ImGui::Text("Volume");
		ImGui::NextColumn();
		float volume = sourceComponent.GetVolume();
		if (ImGui::SliderFloat("##volume", &volume, 0.0f, 1.0f, "%.2f"))
			sourceComponent.SetVolume(volume);
		ImGui::NextColumn();

		ImGui::Text("Mute");
		ImGui::NextColumn();
		bool mute = sourceComponent.GetIsMuted();
		if (ImGui::Checkbox("##mute", &mute))
			sourceComponent.SetIsMuted(mute);
		ImGui::NextColumn();

		ImGui::Text("Pitch");
		ImGui::NextColumn();
		float pitch = sourceComponent.GetPitch();
		if (ImGui::SliderFloat("##pitch", &pitch, -3.0f, 3.0f, "%.2f"))
			sourceComponent.SetPitch(pitch);
		ImGui::NextColumn();

		ImGui::Text("Play On Awake");
		ImGui::NextColumn();
		bool playOnAwake = sourceComponent.GetPlayOnAwake();
		if (ImGui::Checkbox("##playonawake", &playOnAwake))
			sourceComponent.SetPlayOnAwake(playOnAwake);
		ImGui::NextColumn();

		ImGui::Text("Loop");
		ImGui::NextColumn();
		bool looping = sourceComponent.GetLooping();
		if (ImGui::Checkbox("##loop", &looping))
			sourceComponent.SetLooping(looping);
		ImGui::NextColumn();

		ImGui::Text("Min Distance");
		ImGui::NextColumn();
		float minDistance = sourceComponent.GetMinDistance();
		if (ImGui::SliderFloat("##mindistnaceaudio", &minDistance, -3.0f, 3.0f, "%.2f"))
			sourceComponent.SetMinDistance(minDistance);
		ImGui::NextColumn();

		ImGui::Text("Max Distance");
		ImGui::NextColumn();
		float maxDistance = sourceComponent.GetMaxDistance();
		if (ImGui::SliderFloat("##maxdistanceaudio", &maxDistance, -3.0f, 3.0f, "%.2f"))
			sourceComponent.SetMaxDistance(maxDistance);
		ImGui::NextColumn();
		ImGui::Columns(1);
	}

	static void DrawPrimitiveInspector(const Ref<SerializableMemberInfo>& memberInfo, std::function<MonoObject* ()> getter, std::function<void(void*)> setter, const Ref<SerializableTypeInfo>& listTypeInfo = nullptr);
	static void DrawFieldInspector(const Ref<SerializableMemberInfo>& memberInfo, std::function<MonoObject*()> getter, std::function<void(void*)> setter, const Ref<SerializableTypeInfo>& listTypeInfo = nullptr, int depth = 0);
	static void DrawObjectInspector(const Ref<SerializableObjectInfo>& objectInfo, MonoObject* instance, MonoObject* parentInstance, std::function<void(MonoObject*)> = {}, int depth = 0);

	static void DrawListInspector(MonoObject* listObject, const Ref<SerializableMemberInfo>& memberInfo, int depth = 0)
	{
		const Ref<SerializableTypeInfoList>& listInfo = std::static_pointer_cast<SerializableTypeInfoList>(memberInfo->m_TypeInfo);
		MonoClass* listClass = MonoManager::Get().FindClass(MonoUtils::GetClass(listObject));
		if (listClass == nullptr) // this might break the ui
			return;
		MonoProperty* countProp = listClass->GetProperty("Count");
		MonoObject* lengthObj = countProp->Get(listObject);
		if (lengthObj == nullptr)
			return;

		uint32_t length = *(int32_t*)MonoUtils::Unbox(lengthObj);
		MonoProperty* itemProp = listClass->GetProperty("Item");
		ImGui::NextColumn();
		for (uint32_t i = 0; i < length; i++)
		{
			ImGui::SetCursorPosX(ImGui::GetCursorPos().x + (depth + 1) * 15);
			ImGui::Text("%d", i); // change this into treenode?
			ImGui::NextColumn();
			auto getter = [itemProp, i, listObject]() { return itemProp->GetIndexed(listObject, i); };
			auto setter = [itemProp, i, listObject](void* value) { return itemProp->SetIndexed(listObject, i, value); };
			ImGui::PushID(696969 + i); // no way to hit this id right?
			DrawFieldInspector(memberInfo, getter, setter, listInfo->m_ElementType, depth + 1);
		}
		// Draw things for adding and removing elements
		ImGui::NextColumn();
		ImGui::NextColumn();
		ImGui::NextColumn();
		if (ImGui::Button("+"))
		{
			MonoMethod* addMethod = listClass->GetMethod("Add", 1);
			void* elementInstance = MonoUtils::Unbox(itemProp->GetReturnType()->CreateInstance(false));
			void* params[1] = { elementInstance };
			addMethod->Invoke(listObject, params);
		}
		ImGui::SameLine();
		if (length == 0)
			ImGui::BeginDisabled();
		if (ImGui::Button("-"))
		{
			MonoMethod* removeAt = listClass->GetMethod("RemoveAt", 1);
			int lastIdx = length - 1;
			void* params[1] = { &lastIdx };
			removeAt->Invoke(listObject, params);
		}
		if (length == 0)
			ImGui::EndDisabled();
	}

	static void DrawPrimitiveInspector(const Ref<SerializableMemberInfo>& memberInfo, std::function<MonoObject* ()> getter, std::function<void(void*)> setter, const Ref<SerializableTypeInfo>& listInfo)
	{
		const Ref<SerializableTypeInfo>& typeInfo = listInfo == nullptr ? memberInfo->m_TypeInfo : listInfo;
		const Ref<SerializableTypeInfoPrimitive>& primitive = std::static_pointer_cast<SerializableTypeInfoPrimitive>(typeInfo);
		if (primitive->m_Type == ScriptPrimitiveType::String)
		{
			MonoString* value = (MonoString*)getter();
			String stringValue = MonoUtils::FromMonoString(value);
			if (ImGui::InputText("##stringInput", &stringValue))
				setter(MonoUtils::ToMonoString(stringValue));
			ImGui::NextColumn();
			ImGui::PopID();
			return;
		}
		float minValue = -FLT_MAX;
		float maxValue = FLT_MAX;
		int minValueInt = -INT_MAX;
		int maxValueInt = INT_MAX;
		bool displayAsSlider = false;

		void* fieldValue = MonoUtils::Unbox(getter());
		if (memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Range))
		{
			MonoClass* rangeClass = ScriptInfoManager::Get().GetBuiltinClasses().RangeAttribute;
			MonoObject* rangeAttr = memberInfo->GetAttribute(rangeClass);
			rangeClass->GetField("min")->Get(rangeAttr, &minValue);
			rangeClass->GetField("max")->Get(rangeAttr, &maxValue);
			rangeClass->GetField("slider")->Get(rangeAttr, &displayAsSlider);
			minValueInt = (int32_t)minValue;
			maxValueInt = (int32_t)maxValue;
		}

		switch (primitive->m_Type)
		{
		case ScriptPrimitiveType::Bool:
		{
			bool value = *(bool*)fieldValue;
			if (ImGui::Checkbox("##bool", &value))
				setter(&value);
			break;
		}
		case ScriptPrimitiveType::Double:
		{
			float value = (float)*(double*)fieldValue;
			if (UIUtils::DrawFloatControl(value, minValue, maxValue, displayAsSlider))
			{
				double val = value;
				setter(&val);
			}
			break;
		}
		case ScriptPrimitiveType::Float:
		{
			float value = *(float*)fieldValue;
			if (UIUtils::DrawFloatControl(value, minValue, maxValue, displayAsSlider))
				setter(&value);
			break;
		}
		case ScriptPrimitiveType::Char:
		{
			char ar[] = { *(char*)fieldValue, '\0' };
			if (ImGui::InputText("##charField", ar, 2))
				setter(&ar[0]);
			break;
		}
		case ScriptPrimitiveType::I8:
		{
			int8_t value = *(int8_t*)fieldValue;
			int8_t minValue8 = (int8_t)minValueInt;
			int8_t maxValue8 = (int8_t)maxValueInt;
			bool change = false;
			if (!memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Range))
				change = ImGui::DragScalar("##dragInt8", ImGuiDataType_S8, &value, DRAG_SENSITIVITY);
			else if (displayAsSlider)
				change = ImGui::SliderScalar("##sliderInt8", ImGuiDataType_S8, &value, &minValue8, &maxValue8, "%d");
			else
				change = ImGui::DragScalar("##dragInt8", ImGuiDataType_S8, &value, DRAG_SENSITIVITY, &minValue8, &maxValue8);
			value = glm::clamp(value, minValue8, maxValue8);
			if (change)
				setter(&value);
			break;
		}
		case ScriptPrimitiveType::U8:
		{
			uint8_t value = *(uint8_t*)fieldValue;
			uint8_t minValue8 = (uint8_t)minValueInt;
			uint8_t maxValue8 = (uint8_t)maxValueInt;
			bool change = false;
			if (!memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Range))
				change = ImGui::DragScalar("##dragUInt8", ImGuiDataType_U8, &value, DRAG_SENSITIVITY);
			else if (displayAsSlider)
				change = ImGui::SliderScalar("##sliderUInt8", ImGuiDataType_U8, &value, &minValue8, &maxValue8, "%d");
			else
				change = ImGui::DragScalar("##dragUInt8", ImGuiDataType_U8, &value, DRAG_SENSITIVITY, &minValue8, &maxValue8);
			value = glm::clamp(value, minValue8, maxValue8);
			if (change)
				setter(&value);
			break;
		}
		case ScriptPrimitiveType::I16:
		{
			int16_t value = *(int16_t*)fieldValue;
			int16_t minValue16 = (int16_t)minValueInt;
			int16_t maxValue16 = (int16_t)maxValueInt;
			bool change = false;
			if (!memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Range))
				change = ImGui::DragScalar("##dragInt16", ImGuiDataType_S16, &value, DRAG_SENSITIVITY);
			else if (displayAsSlider)
				change = ImGui::SliderScalar("##sliderInt16", ImGuiDataType_S16, &value, &minValue16, &maxValue16, "%d");
			else
				change = ImGui::DragScalar("##dragInt16", ImGuiDataType_S16, &value, DRAG_SENSITIVITY, &minValue16, &maxValue16);
			value = glm::clamp(value, minValue16, maxValue16);
			if (change)
				setter(&value);
			break;
		}
		case ScriptPrimitiveType::U16:
		{
			uint16_t value = *(uint16_t*)fieldValue;
			uint16_t minValue16 = (uint16_t)minValueInt;
			uint16_t maxValue16 = (uint16_t)maxValueInt;
			bool change = false;
			if (!memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Range))
				change = ImGui::DragScalar("##dragUInt16", ImGuiDataType_U16, &value, DRAG_SENSITIVITY);
			else if (displayAsSlider)
				change = ImGui::SliderScalar("##sliderUInt16", ImGuiDataType_U16, &value, &minValue16, &maxValue16, "%d");
			else
				change = ImGui::DragScalar("##dragUInt16", ImGuiDataType_U16, &value, DRAG_SENSITIVITY, &minValue16, &maxValue16);
			value = glm::clamp(value, minValue16, maxValue16);
			if (change)
				setter(&value);
			break;
		}
		case ScriptPrimitiveType::I32:
		{
			int32_t value = *(int32_t*)fieldValue;
			int32_t minValue32 = (int32_t)minValueInt;
			int32_t maxValue32 = (int32_t)maxValueInt;
			bool change = false;
			if (!memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Range))
				change = ImGui::DragScalar("##dragInt32", ImGuiDataType_S32, &value, DRAG_SENSITIVITY);
			else if (displayAsSlider)
				change = ImGui::SliderScalar("##sliderInt32", ImGuiDataType_S32, &value, &minValue32, &maxValue32, "%d");
			else
				change = ImGui::DragScalar("##dragInt32", ImGuiDataType_S32, &value, DRAG_SENSITIVITY, &minValue32, &maxValue32);
			value = glm::clamp(value, minValue32, maxValue32);
			if (change)
				setter(&value);
			break;
		}
		case ScriptPrimitiveType::U32:
		{
			uint32_t value = *(uint32_t*)fieldValue;
			uint32_t minValue32 = (uint32_t)minValueInt;
			uint32_t maxValue32 = (uint32_t)maxValueInt;
			bool change = false;
			if (!memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Range))
				change = ImGui::DragScalar("##dragUInt32", ImGuiDataType_U32, &value, DRAG_SENSITIVITY);
			else if (displayAsSlider)
				change = ImGui::SliderScalar("##sliderUInt32", ImGuiDataType_U32, &value, &minValue32, &maxValue32, "%d");
			else
				change = ImGui::DragScalar("##dragUInt32", ImGuiDataType_U32, &value, DRAG_SENSITIVITY, &minValue32, &maxValue32);
			value = glm::clamp(value, minValue32, maxValue32);
			if (change)
				setter(&value);
			break;
		}
		case ScriptPrimitiveType::I64:
		{
			int64_t value = *(int64_t*)fieldValue;
			int64_t minValue64 = (int64_t)minValueInt;
			int64_t maxValue64 = (int64_t)maxValueInt;
			bool change = false;
			if (!memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Range))
				change = ImGui::DragScalar("##dragInt64", ImGuiDataType_S64, &value, DRAG_SENSITIVITY);
			else if (displayAsSlider)
				change = ImGui::SliderScalar("##sliderInt64", ImGuiDataType_S64, &value, &minValue64, &maxValue64, "%d");
			else
				change = ImGui::DragScalar("##dragInt64", ImGuiDataType_S64, &value, DRAG_SENSITIVITY, &minValue64, &maxValue64);
			value = glm::clamp(value, minValue64, maxValue64);
			if (change)
				setter(&value);
			break;
		}
		case ScriptPrimitiveType::U64:
		{
			uint64_t value = *(uint64_t*)fieldValue;
			uint64_t minValue64 = (uint64_t)minValueInt;
			uint64_t maxValue64 = (uint64_t)maxValueInt;
			bool change = false;
			if (!memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Range))
				change = ImGui::DragScalar("##dragUInt64", ImGuiDataType_U64, &value, DRAG_SENSITIVITY);
			else if (displayAsSlider)
				change = ImGui::SliderScalar("##sliderUInt64", ImGuiDataType_U64, &value, &minValue64, &maxValue64, "%d");
			else
				change = ImGui::DragScalar("##dragUInt64", ImGuiDataType_U64, &value, DRAG_SENSITIVITY, &minValue64, &maxValue64);
			value = glm::clamp(value, minValue64, maxValue64);
			if (change)
				setter(&value);
			break;
		}
		}
		ImGui::NextColumn();
		ImGui::PopID();
	}

	static void DrawEnumInspector(const Ref<SerializableTypeInfoEnum>& enumInfo, std::function<MonoObject* ()> getter, std::function<void(void*)> setter)
	{
		uint32_t value = *(uint32_t*)MonoUtils::Unbox(getter()); // maybe here I would have to check the underlying type.....
		if (value >= enumInfo->m_EnumNames.size())
		{
			ImGui::PopID();
			ImGui::NextColumn();
			return;
		}
		if (ImGui::BeginCombo("##enum", enumInfo->m_EnumNames[value].c_str()))
		{
			uint32_t newSelected = value;
			for (uint32_t i = 0; i < enumInfo->m_EnumNames.size(); i++)
				if (ImGui::Selectable(enumInfo->m_EnumNames[i].c_str(), i == value))
					newSelected = i;

			if (newSelected != value)
			{
				void* tmp = &newSelected;
				setter(tmp);
			}
			ImGui::EndCombo();
		}
		ImGui::PopID();
		ImGui::NextColumn();
	}

	static void DrawFieldInspector(const Ref<SerializableMemberInfo>& memberInfo, std::function<MonoObject*()> getter, std::function<void(void*)> setter, const Ref<SerializableTypeInfo>& listType, int depth)
	{
		const Ref<SerializableTypeInfo>& typeInfo = listType == nullptr ? memberInfo->m_TypeInfo : listType;
		if (typeInfo->GetType() == SerializableType::Enum)
		{
			Ref<SerializableTypeInfoEnum> enumInfo = std::static_pointer_cast<SerializableTypeInfoEnum>(typeInfo);
			DrawEnumInspector(enumInfo, getter, setter);
		}
		else if (typeInfo->GetType() == SerializableType::Primitive)
			DrawPrimitiveInspector(memberInfo, getter, setter, listType);
		else if (typeInfo->GetType() == SerializableType::Array)
		{

		}
		else if (typeInfo->GetType() == SerializableType::List)
		{
			MonoObject* managedList = getter();
			if (managedList == nullptr)
			{
				ImGui::PopID();
				ImGui::NextColumn();
				return;
			}
			DrawListInspector(managedList, memberInfo, depth);
			ImGui::PopID();
			ImGui::NextColumn();
		}
		else if (typeInfo->GetType() == SerializableType::Dictionary)
		{

		}
		else if (typeInfo->GetType() == SerializableType::Asset)
		{

		}
		else if (typeInfo->GetType() == SerializableType::Entity)
		{
			ScriptEntity* scriptEntity = ScriptEntity::ToNative(getter());
			std::string entityName;
			if (scriptEntity == nullptr)
				entityName = "null";
			else
				entityName = scriptEntity->GetNativeEntity().GetName();
			if (ImGui::Button(entityName.c_str(), ImVec2(ImGui::GetContentRegionAvailWidth() * 0.69f, 0))) // Open entity selection modal
				ImGui::OpenPopup("Select entity");

			if (ImGui::BeginDragDropTarget()) // Drag entities on the control
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Entity_ID"))
				{
					CW_ENGINE_ASSERT(payload->DataSize == sizeof(uint32_t));
					uint32_t id = *(const uint32_t*)payload->Data;
					Entity entity{ (entt::entity)id, SceneManager::GetActiveScene().get() };
					MonoObject* value = ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(entity)->GetManagedInstance();
					setter(value);
				}
				ImGui::EndDragDropTarget();
			}
			if (ImGui::BeginPopup("Select entity"))
			{
				Ref<Scene> activeScene = SceneManager::GetActiveScene();
				auto view = activeScene->GetAllEntitiesWith<TagComponent>();
				for (auto id : view)
				{
					Entity entity{ id, activeScene.get() };
					ImGui::PushID((int32_t)id);
					if (ImGui::Selectable(entity.GetName().c_str()))
					{
						MonoObject* value = ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(entity)->GetManagedInstance();
						setter(value);
						ImGui::CloseCurrentPopup();
					}
					ImGui::PopID();
				}
				ImGui::EndPopup();
			}
			ImGui::PopID();
			ImGui::NextColumn();
		}
		else if (typeInfo->GetType() == SerializableType::Object)
		{
			Ref<SerializableObjectInfo> objInfo = nullptr;
			Ref<SerializableTypeInfoObject> objTypeInfo = std::static_pointer_cast<SerializableTypeInfoObject>(typeInfo);
			if (ScriptInfoManager::Get().GetSerializableObjectInfo(objTypeInfo->m_TypeNamespace, objTypeInfo->m_TypeName, objInfo))
			{
				ImGui::NextColumn();
				const Ref<Scene>& scene = SceneManager::GetActiveScene();
				const MonoScriptComponent& comp = Entity(*scene->GetAllEntitiesWith<MonoScriptComponent>().begin(), scene.get()).GetComponent<MonoScriptComponent>();
				if (objTypeInfo->m_Flags.IsSet(ScriptFieldFlagBits::Inspectable))
				{
					if (objTypeInfo->m_ValueType)
					{
						void* obj = getter();
						auto valueTypeSetter = [&](MonoObject* instance) { 
							memberInfo->SetValue(comp.GetManagedInstance(), MonoUtils::Unbox(getter()));
						};
						DrawObjectInspector(objInfo, (MonoObject*)obj, getter(), valueTypeSetter, depth + 1);
					}
					else
						DrawObjectInspector(objInfo, getter(), getter(), {}, depth + 1);
				}
			}
			ImGui::PopID();
			// ImGui::NextColumn();
		}
	}

	// This here does all the work. DrawObjectInspector is called with the MonoObject of the current class instance and draws it all.
	static void DrawObjectInspector(const Ref<SerializableObjectInfo>& objectInfo, MonoObject* instance, MonoObject* parentInstance, std::function<void(MonoObject*)> valueTypeSetter, int depth)
	{
		if (instance == nullptr)
			return;
		for (auto& kv : objectInfo->m_Fields)
		{
			const Ref<SerializableMemberInfo>& memberInfo = kv.second;
			if (!memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Inspectable))
				continue;
			ImGui::PushID(kv.first);
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + depth * 15);
			ImGui::Text(memberInfo->m_Name.c_str()); ImGui::NextColumn();
			MonoObject* val = memberInfo->GetValue(instance);
			auto valueGetter = [&]() { return /*objectInfo->m_TypeInfo->m_ValueType ? instance : */val; };
			auto valueSetter = [&](void* value) {
				memberInfo->SetValue(instance, value);
				if (objectInfo->m_TypeInfo->m_ValueType)
				{
					if (valueTypeSetter)
						valueTypeSetter((MonoObject*)MonoUtils::Unbox(val));
				}
			};
			
			DrawFieldInspector(memberInfo, valueGetter, valueSetter);
		}
	}

	template <> void ComponentEditorWidget<MonoScriptComponent>(Entity e)
	{
		MonoScriptComponent& script = e.GetComponent<MonoScriptComponent>();
		ImGui::Columns(2);
		ImGui::Text("Script");
		ImGui::NextColumn();

		if (!script.GetManagedClass())
		{
			ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(255, 0, 0));
		}
		else
			ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(0, 255, 0));
		String name = script.GetManagedClass() ? String(script.GetManagedClass()->GetName()) : "";
		if (ImGui::InputText("##scriptName", &name))
		{
			script.SetClassName(name);
			script.OnInitialize(e);
		}

		ImGui::PopStyleColor(1);

		if (script.GetManagedClass() == nullptr)
		{
			ImGui::Columns(1);
			return;
		}

		ImGui::NextColumn();
		Ref<SerializableObjectInfo> objectInfo = script.GetObjectInfo();
		MonoObject* instance = script.GetManagedInstance();
		DrawObjectInspector(objectInfo, instance, nullptr);
		/*
			
			case (MonoPrimitiveType::Class):
			{
				if (fieldClass->IsSubClassOf(ScriptInfoManager::Get().GetBuiltinClasses().EntityClass)) // Entity handle
				{
					if (ImGui::Button("Select entity")) // Open entity selection modal
						ImGui::OpenPopup(field->GetName().c_str());

					if (ImGui::BeginDragDropTarget()) // Drag entities on the control
					{
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Entity_ID"))
						{
							CW_ENGINE_ASSERT(payload->DataSize == sizeof(uint32_t));
							uint32_t id = *(const uint32_t*)payload->Data;
							Entity entity{ (entt::entity)id, SceneManager::GetActiveScene().get() };
							MonoObject* value = ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(entity)->GetManagedInstance();
							field->Set(instance, value);
						}
						ImGui::EndDragDropTarget();
					}
				}
				break;
				}
		*/
		ImGui::Columns(1);
	}

} // namespace Crowny

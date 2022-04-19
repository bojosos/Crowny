#include "cwepch.h"

#include "UI/ScriptInspector.h"
#include "UI/Properties.h"

#include "Crowny/Scripting/Mono/MonoArray.h"
#include "Crowny/Scripting/ScriptInfoManager.h"

#include <imgui.h>

namespace Crowny
{
	void ScriptInspector::DrawListInspector(MonoObject* listObject, const Ref<SerializableMemberInfo>& memberInfo, int depth)
    {
        const Ref<SerializableTypeInfoList>& listInfo =
          std::static_pointer_cast<SerializableTypeInfoList>(memberInfo->m_TypeInfo);
        MonoClass* listClass = MonoManager::Get().FindClass(MonoUtils::GetClass(listObject));
        MonoProperty* countProp = listClass->GetProperty("Count");
        MonoObject* lengthObj = countProp->Get(listObject);
        uint32_t length = *(int32_t*)MonoUtils::Unbox(lengthObj);
        MonoProperty* itemProp = listClass->GetProperty("Item");
		MonoMethod* copyToMethod = listClass->GetMethod("CopyTo", 4);
		MonoMethod* addRangeMethod = listClass->GetMethod("AddRange", 1);
		MonoMethod* clearMethod = listClass->GetMethod("Clear");
		uint32_t newLength = length;
        if (UI::PropertyInput(memberInfo->m_Name.c_str(), newLength))
		{
			ScriptArray tempArray(itemProp->GetReturnType()->GetInternalPtr(), newLength);
			uint32_t minSize = std::min(length, newLength);
			uint32_t start = 0;

			void* params[4];
			params[0] = &start;
			params[1] = tempArray.GetInternal();
			params[2] = &start;
			params[3] = &minSize;

			copyToMethod->Invoke(listObject, params);
			clearMethod->Invoke(listObject, nullptr);

			params[0] = tempArray.GetInternal();
			addRangeMethod->Invoke(listObject, params);
		}
        for (uint32_t i = 0; i < newLength; i++)
        {
            ImGui::SetCursorPosX(ImGui::GetCursorPos().x + (depth + 1) * 25);
            auto getter = [itemProp, i, listObject]() { return itemProp->GetIndexed(listObject, i); };
            auto setter = [itemProp, i, listObject](void* value) { return itemProp->SetIndexed(listObject, i, value); };
            DrawFieldInspector(memberInfo, std::to_string(i).c_str(), getter, setter, listInfo->m_ElementType, depth + 1);
        }
    }

    void ScriptInspector::DrawPrimitiveInspector(const Ref<SerializableMemberInfo>& memberInfo, const char* label,
                                       std::function<MonoObject*()> getter, std::function<void(void*)> setter,
                                       const Ref<SerializableTypeInfo>& listInfo)
    {
        const Ref<SerializableTypeInfo>& typeInfo = listInfo == nullptr ? memberInfo->m_TypeInfo : listInfo;
        const Ref<SerializableTypeInfoPrimitive>& primitive =
          std::static_pointer_cast<SerializableTypeInfoPrimitive>(typeInfo);
        if (primitive->m_Type == ScriptPrimitiveType::String)
        {
            MonoString* value = (MonoString*)getter();
            String stringValue = MonoUtils::FromMonoString(value);
            if (UI::Property(label, stringValue))
                setter(MonoUtils::ToMonoString(stringValue));
            return;
        }
        float minValue = -FLT_MAX;
        float maxValue = FLT_MAX;
        int64_t minValueInt = -LONG_MAX;
        int64_t maxValueInt = LONG_MAX;
        bool displayAsSlider = false;

        void* fieldValue = MonoUtils::Unbox(getter());
        if (memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Range))
        {
            MonoClass* rangeClass = ScriptInfoManager::Get().GetBuiltinClasses().RangeAttribute;
            MonoObject* rangeAttr = memberInfo->GetAttribute(rangeClass);
            rangeClass->GetField("min")->Get(rangeAttr, &minValue);
            rangeClass->GetField("max")->Get(rangeAttr, &maxValue);
            rangeClass->GetField("slider")->Get(rangeAttr, &displayAsSlider);
            displayAsSlider = false;
            minValueInt = (int32_t)minValue;
            maxValueInt = (int32_t)maxValue;
        }

		/*if (listInfo == nullptr && primitive->m_Type != ScriptPrimitiveType::Bool && primitive->m_Type != ScriptPrimitiveType::Float && primitive->m_Type != ScriptPrimitiveType::Double)
		{
			ImGui::Text(label);
			ImGui::NextColumn();
		}*/

        switch (primitive->m_Type)
        {
        case ScriptPrimitiveType::Bool: {
            bool value = *(bool*)fieldValue;
            if (UI::Property(label, value))
                setter(&value);
            break;
        }
        case ScriptPrimitiveType::Double: {
            float value = (float)*(double*)fieldValue;
            if (UIUtils::DrawFloatControl(label, value, minValue, maxValue, displayAsSlider))
            {
                double val = value;
                setter(&val);
            }
            break;
        }
        case ScriptPrimitiveType::Float: {
            float value = *(float*)fieldValue;
            if (UIUtils::DrawFloatControl(label, value, minValue, maxValue, displayAsSlider))
                setter(&value);
            break;
        }
        case ScriptPrimitiveType::Char: {
            char ar[] = { *(char*)fieldValue, '\0' };
            String str(ar);
            if (UI::Property(label, str)) // meh
            {
                str.resize(1);
                setter(&ar[0]);
            }
            break;
        }
        case ScriptPrimitiveType::I8: {
            int8_t value = *(int8_t*)fieldValue;
            int8_t minValue8 = glm::clamp(minValueInt, -128LL, 127LL);
            int8_t maxValue8 = glm::clamp(maxValueInt, -128LL, 127LL);
            bool change = false;
            if (!memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Range))
                change = UI::Property(label, value, minValue8, maxValue8);
            else if (displayAsSlider)
                change = ImGui::SliderScalar("##sliderInt8", ImGuiDataType_S8, &value, &minValue8, &maxValue8, "%d");
            else
                change = UI::Property(label, value, minValue8, maxValue8);
            if (change)
			{
				value = glm::clamp((int32_t)value, (int32_t)minValue8, (int32_t)maxValue8);
                setter(&value);
            }
            break;
        }
        case ScriptPrimitiveType::U8: {
            uint8_t value = *(uint8_t*)fieldValue;
            uint8_t minValue8 = glm::clamp(minValueInt, 0LL, 255LL);
            uint8_t maxValue8 = glm::clamp(maxValueInt, 0LL, 255LL);
            bool change = false;
            if (!memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Range))
                change = UI::Property(label, value, minValue8, maxValue8);
            else if (displayAsSlider)
                change = ImGui::SliderScalar("##sliderUInt8", ImGuiDataType_U8, &value, &minValue8, &maxValue8, "%d");
            else
                change = UI::Property(label, value, minValue8, maxValue8);
            if (change)
			{
                CW_ENGINE_INFO("{0}, {1}, {2}", (int32_t)value, (int32_t)minValue8, (int32_t)maxValue8);
				value = glm::clamp((int32_t)value, (int32_t)minValue8, (int32_t)maxValue8);
                setter(&value);
            }
            break;
        }
        case ScriptPrimitiveType::I16: {
            int16_t value = *(int16_t*)fieldValue;
            int16_t minValue16 = glm::clamp(minValueInt, -32768LL, 32767LL);
            int16_t maxValue16 = glm::clamp(maxValueInt, -32768LL, 32767LL);
            bool change = false;
            if (!memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Range))
                change = UI::Property(label, value, minValue16, maxValue16);
                // change = ImGui::DragScalar("##dragInt16", ImGuiDataType_S16, &value, DRAG_SENSITIVITY);
            else if (displayAsSlider)
                change =
                  ImGui::SliderScalar("##sliderInt16", ImGuiDataType_S16, &value, &minValue16, &maxValue16, "%d");
            else
                // change = ImGui::DragScalar("##dragInt16", ImGuiDataType_S16, &value, DRAG_SENSITIVITY, &minValue16,
                   //                        &maxValue16);
            value = glm::clamp((int32_t)value, (int32_t)minValue16, (int32_t)maxValue16);
            if (change)
                setter(&value);
            break;
        }
        case ScriptPrimitiveType::U16: {
            uint16_t value = *(uint16_t*)fieldValue;
            uint16_t minValue16 = glm::clamp(minValueInt, 0LL, 65535LL);
            uint16_t maxValue16 = glm::clamp(maxValueInt, 0LL, 65535LL);
            bool change = false;
            if (!memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Range))
                change = UI::Property(label, value, minValue16, maxValue16);
                // change = ImGui::DragScalar("##dragUInt16", ImGuiDataType_U16, &value, DRAG_SENSITIVITY);
            else if (displayAsSlider)
                change =
                  ImGui::SliderScalar("##sliderUInt16", ImGuiDataType_U16, &value, &minValue16, &maxValue16, "%d");
            else
                // change = ImGui::DragScalar("##dragUInt16", ImGuiDataType_U16, &value, DRAG_SENSITIVITY, &minValue16,
                   //                        &maxValue16);
                change = UI::Property(label, value, minValue16, maxValue16);
            if (change)
			{
				value = glm::clamp((int32_t)value, (int32_t)minValue16, (int32_t)maxValue16);
                setter(&value);
            }
            break;
        }
        case ScriptPrimitiveType::I32: {
            int32_t value = *(int32_t*)fieldValue;
			int32_t minValue32 = glm::clamp(minValueInt, -2147483648LL, 2147483647LL);
			int32_t maxValue32 = glm::clamp(maxValueInt, -2147483648LL, 2147483647LL);
            bool change = false;
            if (!memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Range))
                change = UI::Property(label, value, minValue32, maxValue32);
            else if (displayAsSlider)
                change =
                  ImGui::SliderScalar("##sliderInt32", ImGuiDataType_S32, &value, &minValue32, &maxValue32, "%d");
            else
                change = UI::Property(label, value, minValue32, maxValue32);
            if (change)
			{
				value = glm::clamp((int64_t)value, (int64_t)minValue32, (int64_t)maxValue32);
                setter(&value);
            }
            break;
        }
        case ScriptPrimitiveType::U32: {
            uint32_t value = *(uint32_t*)fieldValue;
			uint32_t minValue32 = glm::clamp(minValueInt, 0LL, 4294967295LL);
			uint32_t maxValue32 = glm::clamp(maxValueInt, 0LL, 4294967295LL);
            bool change = false;
            if (!memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Range))
                change = UI::Property(label, value, minValue32, maxValue32);
            else if (displayAsSlider)
                change =
                  ImGui::SliderScalar("##sliderUInt32", ImGuiDataType_U32, &value, &minValue32, &maxValue32, "%d");
            else
                change = UI::Property(label, value, minValue32, maxValue32);
            if (change)
			{
				CW_ENGINE_INFO("{0}, {1}, {2}", (int64_t)value, (int64_t)minValue32, (int64_t)maxValue32);
				value = glm::clamp((int64_t)value, (int64_t)minValue32, (int64_t)maxValue32);
                setter(&value);
            }
            break;
        }
        case ScriptPrimitiveType::I64: {
            int64_t value = *(int64_t*)fieldValue;
            int64_t minValue64 = (int64_t)minValueInt;
            int64_t maxValue64 = (int64_t)maxValueInt;
            bool change = false;
            if (!memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Range))
                change = UI::Property(label, value, minValue64, maxValue64);
            else if (displayAsSlider)
                change =
                  ImGui::SliderScalar("##sliderInt64", ImGuiDataType_S64, &value, &minValue64, &maxValue64, "%d");
            else
                change = UI::Property(label, value, minValue64, maxValue64);
            if (change)
			{
				value = glm::clamp(value, minValue64, maxValue64);
                setter(&value);
            }
            break;
        }
        case ScriptPrimitiveType::U64: {
            uint64_t value = *(uint64_t*)fieldValue;
			uint64_t minValue64 = glm::clamp(minValueInt, 0LL, 4294967295LL);
			uint64_t maxValue64 = glm::clamp(maxValueInt, 0LL, 4294967295LL);
            bool change = false;
            if (!memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Range))
                change = UI::Property(label, value, minValue64, maxValue64);
            else if (displayAsSlider)
                change =
                  ImGui::SliderScalar("##sliderUInt64", ImGuiDataType_U64, &value, &minValue64, &maxValue64, "%d");
            else
                change = UI::Property(label, value, minValue64, maxValue64);
            if (change)
			{
				value = glm::clamp(value, minValue64, maxValue64);
                setter(&value);
            }
            break;
        }
        case ScriptPrimitiveType::Vector2: {
            glm::vec2 value = *(glm::vec2*)fieldValue;
            if (UI::Property(label, value))
				setter(&value);
			break;
		}
		case ScriptPrimitiveType::Vector3: {
			glm::vec3 value = *(glm::vec3*)fieldValue;
			if (UI::Property(label, value))
				setter(&value);
            break;
		}
		case ScriptPrimitiveType::Vector4: {
			glm::vec4 value = *(glm::vec4*)fieldValue;
			if (UI::Property(label, value))
				setter(&value);
			break;
		}
        }
    }

    void ScriptInspector::DrawEnumInspector(const Ref<SerializableMemberInfo>& memberInfo, const Ref<SerializableTypeInfoEnum>& enumInfo, std::function<MonoObject*()> getter,
                                  std::function<void(void*)> setter)
    {
        uint32_t value =
          *(uint32_t*)MonoUtils::Unbox(getter()); // maybe here I would have to check the underlying type.....
        if (value >= enumInfo->m_EnumNames.size())
        {
            ImGui::NextColumn();
            return;
        }
        if (UI::PropertyDropdown(memberInfo->m_Name.c_str(), enumInfo->m_EnumNames, value))
			setter(&value);
    }

    void ScriptInspector::DrawFieldInspector(const Ref<SerializableMemberInfo>& memberInfo, const char* label, std::function<MonoObject*()> getter,
                                   std::function<void(void*)> setter, const Ref<SerializableTypeInfo>& listType,
                                   int depth)
    {
        const Ref<SerializableTypeInfo>& typeInfo = listType == nullptr ? memberInfo->m_TypeInfo : listType;
        if (typeInfo->GetType() == SerializableType::Enum)
        {
            Ref<SerializableTypeInfoEnum> enumInfo = std::static_pointer_cast<SerializableTypeInfoEnum>(typeInfo);
            DrawEnumInspector(memberInfo, enumInfo, getter, setter);
        }
        else if (typeInfo->GetType() == SerializableType::Primitive)
            DrawPrimitiveInspector(memberInfo, label, getter, setter, listType);
        else if (typeInfo->GetType() == SerializableType::Array)
        {
			MonoArray* monoAr = (MonoArray*)getter();
			if (monoAr != nullptr)
            {
			    ScriptArray ar = ScriptArray(monoAr);
                uint32_t size = ar.Size();
                if (UI::PropertyInput(label, size))
                {
					ar.Resize(size);
					setter(ar.GetInternal());
                }
            }
        }
        else if (typeInfo->GetType() == SerializableType::List)
        {
            MonoObject* managedList = getter();
            if (managedList == nullptr) // If not user initialized, initialize ourself
            {
				MonoClass* klass = MonoManager::Get().FindClass(memberInfo->m_TypeInfo->GetMonoClass());
                managedList = klass->CreateInstance(true);
                setter(managedList);
            }
            DrawListInspector(managedList, memberInfo, depth);
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
			
            Entity entity = { entt::null, nullptr };
            if (scriptEntity != nullptr)
                entity = scriptEntity->GetNativeEntity();
            if (UIUtils::EntityReference(memberInfo->m_Name, entity))
            {
                if (entity)
                {
				    MonoObject* value =
					    ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(entity)->GetManagedInstance();
				    setter(value);
                }
                else
					setter(nullptr);
            }
        }
        else if (typeInfo->GetType() == SerializableType::Object)
        {
            Ref<SerializableObjectInfo> objInfo = nullptr;
            Ref<SerializableTypeInfoObject> objTypeInfo =
              std::static_pointer_cast<SerializableTypeInfoObject>(typeInfo);
			ImGui::Text(memberInfo->m_Name.c_str());
            if (ScriptInfoManager::Get().GetSerializableObjectInfo(objTypeInfo->m_TypeNamespace,
                                                                   objTypeInfo->m_TypeName, objInfo))
            {
                ImGui::NextColumn();
                ImGui::NextColumn();
                const Ref<Scene>& scene = SceneManager::GetActiveScene();
                const MonoScriptComponent& comp =
                  Entity(*scene->GetAllEntitiesWith<MonoScriptComponent>().begin(), scene.get())
                    .GetComponent<MonoScriptComponent>(); // wtf was I thinking?
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
            // ImGui::NextColumn();
        }
    }

    // This here does all the work. DrawObjectInspector is called with the MonoObject of the current class instance and
    // draws it all.
    void ScriptInspector::DrawObjectInspector(const Ref<SerializableObjectInfo>& objectInfo, MonoObject* instance,
                                    MonoObject* parentInstance, std::function<void(MonoObject*)> valueTypeSetter,
                                    int depth)
    {
        if (instance == nullptr)
            return;
        for (auto& kv : objectInfo->m_Fields)
        {
            const Ref<SerializableMemberInfo>& memberInfo = kv.second;
            if (!memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Inspectable))
                continue;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + depth * 25);
            MonoObject* val = memberInfo->GetValue(instance);
            auto valueGetter = [&]() { return val; };
            auto valueSetter = [&](void* value) {
                memberInfo->SetValue(instance, value);
                if (objectInfo->m_TypeInfo->m_ValueType)
                {
                    if (valueTypeSetter)
                        valueTypeSetter((MonoObject*)MonoUtils::Unbox(val));
                }
            };

            DrawFieldInspector(memberInfo, memberInfo->m_Name.c_str(), valueGetter, valueSetter);
        }
    }
}
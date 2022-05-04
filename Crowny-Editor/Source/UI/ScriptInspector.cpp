#include "cwepch.h"

#include "UI/Properties.h"
#include "UI/ScriptInspector.h"

#include "Crowny/Scripting/Mono/MonoArray.h"
#include "Crowny/Scripting/ScriptAssetManager.h"
#include "Crowny/Scripting/ScriptInfoManager.h"

#include <imgui.h>

namespace Crowny
{
    // Note: setter is needed for null lists, since they are created only if a new element is added
    bool ScriptInspector::DrawListInspector(MonoObject* listObject, const Ref<SerializableMemberInfo>& memberInfo,
                                            std::function<void(void*)> setter, int depth)
    {
        const Ref<SerializableTypeInfoList>& listInfo =
          std::static_pointer_cast<SerializableTypeInfoList>(memberInfo->m_TypeInfo);
        bool modified = false;
        MonoClass* listClass = MonoManager::Get().FindClass(listInfo->GetMonoClass());
        MonoProperty* countProp = listClass->GetProperty("Count");
        uint32_t length = 0;
        if (listObject != nullptr)
        {
            MonoObject* lengthObj = countProp->Get(listObject);
            length = *(int32_t*)MonoUtils::Unbox(lengthObj);
        }
        MonoProperty* itemProp = listClass->GetProperty("Item");
        MonoMethod* copyToMethod = listClass->GetMethod("CopyTo", 4);
        MonoMethod* addRangeMethod = listClass->GetMethod("AddRange", 1);
        MonoMethod* clearMethod = listClass->GetMethod("Clear");
        uint32_t newLength = length;
        if (UI::PropertyInput(memberInfo->m_Name.c_str(), newLength))
        {
            if (listObject == nullptr && newLength != 0) // If not user initialized, initialize ourself
            {
                MonoClass* klass = MonoManager::Get().FindClass(
                  memberInfo->m_TypeInfo->GetMonoClass()); // So this somehow works without having to bind parameters
                // MonoClass* klass = ScriptInfoManager::Get().GetBuiltinClasses().SystemGenericListClass;
                listObject = klass->CreateInstance(true);
                setter(listObject);
            }
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
            if (listInfo->m_ElementType->GetType() == SerializableType::Object)
            {
                auto objInfo = std::static_pointer_cast<SerializableTypeInfoObject>(listInfo->m_ElementType);
                if (!objInfo->m_ValueType)
                {
                    for (uint32_t i = length; i < newLength; i++) // Maybe worth calling the constructor if one exists?
                        itemProp->SetIndexed(listObject, i, itemProp->GetReturnType()->CreateInstance(false));
                }
            }
            modified = true;
        }
        for (uint32_t i = 0; i < newLength; i++)
        {
            ImGui::SetCursorPosX(ImGui::GetCursorPos().x + (depth + 1) * 25);
            auto getter = [itemProp, i, listObject]() { return itemProp->GetIndexed(listObject, i); };
            auto setter = [itemProp, i, listObject](void* value) { return itemProp->SetIndexed(listObject, i, value); };
            modified |= DrawFieldInspector(memberInfo, std::to_string(i).c_str(), getter, setter,
                                           listInfo->m_ElementType, depth + 1);
        }
        return modified;
    }

    bool ScriptInspector::DrawDictionaryInspector(MonoObject* dictObject, const Ref<SerializableMemberInfo>& memberInfo,
                                                  std::function<void(void*)> setter, int depth)
    {
        ImGui::PushID(depth);
        const Ref<SerializableTypeInfoDictionary>& dictInfo =
          std::static_pointer_cast<SerializableTypeInfoDictionary>(memberInfo->m_TypeInfo);
        bool modified = false;
        MonoClass* dictClass = MonoManager::Get().FindClass(dictInfo->GetMonoClass());
        MonoProperty* countProp = dictClass->GetProperty("Count");
        MonoProperty* keysProp = dictClass->GetProperty("Keys");
        MonoProperty* valuesProp = dictClass->GetProperty("Values");
        uint32_t length = 0;
        if (dictObject != nullptr)
        {
            MonoObject* lengthObj = countProp->Get(dictObject);
            length = *(int32_t*)MonoUtils::Unbox(lengthObj);
        }
        ScriptArray keys(dictInfo->m_KeyType->GetMonoClass(), length);
        ScriptArray values(dictInfo->m_ValueType->GetMonoClass(), length);
        MonoMethod* copyKeysToMethod = keysProp->GetReturnType()->GetMethod("CopyTo", 2);
        MonoMethod* copyValuesToMethod = valuesProp->GetReturnType()->GetMethod("CopyTo", 2);
        MonoMethod* containsKey = dictClass->GetMethod("ContainsKey", 1);
        MonoMethod* addMethod = dictClass->GetMethod("Add", 2);
        MonoMethod* removeMethod = dictClass->GetMethod("Remove", 1);

        uint32_t offset = 0;
        void* params[2];
        params[0] = keys.GetInternal();
        params[1] = &offset;
        copyKeysToMethod->Invoke(keysProp->Get(dictObject), params);
        params[0] = values.GetInternal();
        copyValuesToMethod->Invoke(valuesProp->Get(dictObject), params);
        int32_t keyInt = 0;
        String keyString;
        ImGui::Columns(3);
        bool isKeyString = dictInfo->m_KeyType->GetMonoClass() == MonoUtils::GetStringClass();
        bool isKeyInt = dictInfo->m_KeyType->GetMonoClass() == MonoUtils::GetI32Class();
        for (uint32_t i = 0; i < length; i++)
        {
            ImGui::SetCursorPosX(ImGui::GetCursorPos().x + (depth + 1) * 25);
            auto getterValue = [&]() {
                if (MonoManager::Get().FindClass(values.GetElementClass())->IsValueType())
                    return MonoUtils::Box(values.GetElementClass(), values.GetRaw(i, values.ElementSize()));
                return values.Get<MonoObject*>(i);
            };
            auto setterValue = [isKeyString, i, dictObject, addMethod, removeMethod, dictInfo, keys,
                                params](void* value) mutable {
                params[1] = value;
                if (isKeyString)
                {
                    params[0] = MonoUtils::ToMonoString(keys.Get<String>(i));
                    removeMethod->Invoke(dictObject, params);
                    addMethod->Invoke(dictObject, params);
                }
                else
                {
                    int32_t key = keys.Get<int32_t>(i);
                    params[0] = &key;
                    removeMethod->Invoke(dictObject, params);
                    addMethod->Invoke(dictObject, params);
                }
            };
            if (isKeyString)
            {
                keyString = keys.Get<String>(i);
                modified |= DrawFieldInspector(memberInfo, keyString.c_str(), getterValue, setterValue,
                                               dictInfo->m_ValueType, depth + 1);
                UI::Underline(true);
                params[0] = MonoUtils::ToMonoString(keyString);
            }
            else if (isKeyInt)
            {
                keyInt = keys.Get<uint32_t>(i);
                modified |= DrawFieldInspector(memberInfo, std::to_string(keyInt).c_str(), getterValue, setterValue,
                                               dictInfo->m_ValueType, depth + 1);
                UI::Underline(true);
                params[0] = &keyInt;
            }
            if (ImGui::Button(UI::GenerateLabelID("-")))
                removeMethod->Invoke(dictObject, params);
            ImGui::NextColumn();
        }
        if (isKeyInt)
        {
            static int32_t keyInt = 0;
            static String valueString;
            ImGui::PushItemWidth(-1);
            UI::PropertyDictionary(keyInt, valueString);
            params[0] = &keyInt;
            MonoObject* containsObject = containsKey->Invoke(dictObject, params);
            bool contains = *(bool*)MonoUtils::Unbox(containsObject);
            ImGui::BeginDisabled(contains);
            if (ImGui::Button(UI::GenerateLabelID("Add")))
            {
                params[1] = MonoUtils::ToMonoString(valueString);
                addMethod->Invoke(dictObject, params);
            }
            ImGui::EndDisabled();
        }
        else if (isKeyString)
        {
            static String keyString;
            static int valueInt = 0;
            ImGui::PushItemWidth(-1);
            UI::PropertyDictionary(keyString, valueInt);
            params[0] = MonoUtils::ToMonoString(keyString);
            MonoObject* containsObject = containsKey->Invoke(dictObject, params);
            bool contains = *(bool*)MonoUtils::Unbox(containsObject);
            ImGui::BeginDisabled(contains);
            if (ImGui::Button(UI::GenerateLabelID("Add")))
            {
                params[1] = &valueInt;
                addMethod->Invoke(dictObject, params);
            }
            ImGui::EndDisabled();
        }
        ImGui::Columns(2);
        ImGui::PopID();
        return false;
    }

    bool ScriptInspector::DrawPrimitiveInspector(const Ref<SerializableMemberInfo>& memberInfo, const char* label,
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
			if (memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Filepath))
            {
                if (UI::PropertyFilepath(label, stringValue))
                {
					setter(MonoUtils::ToMonoString(stringValue));
					return true;
                }
            }
            else
            if (UI::Property(label, stringValue))
            {
                setter(MonoUtils::ToMonoString(stringValue));
                return true;
            }
            return false;
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
        bool modified = false;
        switch (primitive->m_Type)
        {
        case ScriptPrimitiveType::Bool: {
            bool value = *(bool*)fieldValue;
            if (memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Dropdown))
            {
				static Vector<const char*> options = { "False", "True" };
                if (UI::PropertyDropdown(label, options, value))
                {
					modified = true;
					setter(&value);
                    return true;
                }
            }
            else if (UI::Property(label, value))
            {
                modified = true;
                setter(&value);
                return true;
            }
            break;
        }
        case ScriptPrimitiveType::Double: {
            float value = (float)*(double*)fieldValue;
            if (UIUtils::DrawFloatControl(label, value, minValue, maxValue, displayAsSlider))
            {
                modified = true;
                double val = value;
                setter(&val);
            }
            break;
        }
        case ScriptPrimitiveType::Float: {
            float value = *(float*)fieldValue;
            if (UIUtils::DrawFloatControl(label, value, minValue, maxValue, displayAsSlider))
            {
                modified = true;
                setter(&value);
            }
            break;
        }
        case ScriptPrimitiveType::Char: {
            char ar[] = { *(char*)fieldValue, '\0' };
            String str(ar);
            if (UI::Property(label, str)) // meh
            {
                modified = true;
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
                modified = true;
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
                modified = true;
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
            else if (displayAsSlider)
                change =
                  ImGui::SliderScalar("##sliderInt16", ImGuiDataType_S16, &value, &minValue16, &maxValue16, "%d");
            else
                value = glm::clamp((int32_t)value, (int32_t)minValue16, (int32_t)maxValue16);
            if (change)
            {
                modified = true;
                setter(&value);
            }
            break;
        }
        case ScriptPrimitiveType::U16: {
            uint16_t value = *(uint16_t*)fieldValue;
            uint16_t minValue16 = glm::clamp(minValueInt, 0LL, 65535LL);
            uint16_t maxValue16 = glm::clamp(maxValueInt, 0LL, 65535LL);
            bool change = false;
            if (!memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Range))
                change = UI::Property(label, value, minValue16, maxValue16);
            else if (displayAsSlider)
                change =
                  ImGui::SliderScalar("##sliderUInt16", ImGuiDataType_U16, &value, &minValue16, &maxValue16, "%d");
            else
                change = UI::Property(label, value, minValue16, maxValue16);
            if (change)
            {
                modified = true;
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
                modified = true;
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
                modified = true;
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
                modified = true;
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
                modified = true;
                setter(&value);
            }
            break;
        }
        case ScriptPrimitiveType::Vector2: {
            glm::vec2 value = *(glm::vec2*)fieldValue;
            if (UI::Property(label, value))
            {
                setter(&value);
                modified = true;
            }
            break;
        }
        case ScriptPrimitiveType::Vector3: {
            glm::vec3 value = *(glm::vec3*)fieldValue;
            if (UI::Property(label, value))
            {
                setter(&value);
                modified = true;
            }
            break;
        }
        case ScriptPrimitiveType::Vector4: {
            glm::vec4 value = *(glm::vec4*)fieldValue;
            if (UI::Property(label, value))
            {
                setter(&value);
                modified = true;
            }
            break;
        }
        case ScriptPrimitiveType::Matrix4: {
            glm::mat4 value = *(glm::mat4*)fieldValue;
            glm::vec4 r0 = { value[0][0], value[1][0], value[2][0], value[3][0] };
            glm::vec4 r1 = { value[0][1], value[1][1], value[2][1], value[3][1] };
            glm::vec4 r2 = { value[0][2], value[1][2], value[2][2], value[3][2] };
            glm::vec4 r3 = { value[0][3], value[1][3], value[2][3], value[3][3] };
            UI::Property(memberInfo->m_Name.c_str());
            UI::ShiftCursorX(25.0f);
            if (UI::Property("Row 1", r0))
            {
                value[0][0] = r0[0];
                value[1][0] = r0[1];
                value[2][0] = r0[2];
                value[3][0] = r0[3];
                setter(&value);
                modified = true;
            }
            UI::ShiftCursorX(25.0f);
            if (UI::Property("Row 2", r1))
            {
                value[0][1] = r1[0];
                value[1][1] = r1[1];
                value[2][1] = r1[2];
                value[3][1] = r1[3];
                setter(&value);
                modified = true;
            }
            UI::ShiftCursorX(25.0f);
            if (UI::Property("Row 3", r2))
            {
                value[0][2] = r2[0];
                value[1][2] = r2[1];
                value[2][2] = r2[2];
                value[3][2] = r2[3];
                setter(&value);
                modified = true;
            }
            UI::ShiftCursorX(25.0f);
            if (UI::Property("Row 4", r3))
            {
                value[0][3] = r3[0];
                value[1][3] = r3[1];
                value[2][3] = r3[2];
                value[3][3] = r3[3];
                setter(&value);
                modified = true;
            }
            break;
        }
        }
        return modified;
    }

    bool ScriptInspector::DrawEnumInspector(const Ref<SerializableMemberInfo>& memberInfo,
                                            const Ref<SerializableTypeInfoEnum>& enumInfo,
                                            std::function<MonoObject*()> getter, std::function<void(void*)> setter)
    {
        uint32_t value =
          *(uint32_t*)MonoUtils::Unbox(getter());  // maybe here I would have to check the underlying type.....
        if (value >= enumInfo->m_EnumNames.size()) // Maybe clamp the value here?
        {
            ImGui::NextColumn();
            return false;
        }
        if (UI::PropertyDropdown(memberInfo->m_Name.c_str(), enumInfo->m_EnumNames, value))
        {
            setter(&value);
            return true;
        }
    }

    bool ScriptInspector::DrawFieldInspector(const Ref<SerializableMemberInfo>& memberInfo, const char* label,
                                             std::function<MonoObject*()> getter, std::function<void(void*)> setter,
                                             const Ref<SerializableTypeInfo>& listType, int depth)
    {
		UI::ScopedDisable disabled(memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::ReadOnly));
        const Ref<SerializableTypeInfo>& typeInfo = listType == nullptr ? memberInfo->m_TypeInfo : listType;
        if (typeInfo->GetType() == SerializableType::Enum)
        {
            Ref<SerializableTypeInfoEnum> enumInfo = std::static_pointer_cast<SerializableTypeInfoEnum>(typeInfo);
            return DrawEnumInspector(memberInfo, enumInfo, getter, setter);
        }
        else if (typeInfo->GetType() == SerializableType::Primitive)
            return DrawPrimitiveInspector(memberInfo, label, getter, setter, listType);
        else if (typeInfo->GetType() == SerializableType::Array)
        {
            MonoArray* monoAr = (MonoArray*)getter();
            if (monoAr != nullptr)
            {
                ScriptArray ar = ScriptArray(monoAr);
                uint32_t size = ar.Size();
                if (UI::PropertyInput(label, size))
                {
                    return true;
                    ar.Resize(size);
                    setter(ar.GetInternal());
                }
            }
            return false;
        }
        else if (typeInfo->GetType() == SerializableType::List)
            return DrawListInspector(getter(), memberInfo, setter, depth);
        else if (typeInfo->GetType() == SerializableType::Dictionary)
            return DrawDictionaryInspector(getter(), memberInfo, setter, depth);
        else if (typeInfo->GetType() == SerializableType::Asset)
        {
            Ref<SerializableTypeInfoAsset> assetInfo = std::static_pointer_cast<SerializableTypeInfoAsset>(typeInfo);
            ScriptAsset* scriptAsset = ScriptAsset::ToNative(getter());
            AssetHandle<Asset> handle;
            if (scriptAsset != nullptr)
                handle = scriptAsset->GetGenericHandle();
            if (UIUtils::AssetReference(memberInfo->m_Name.c_str(), handle, assetInfo->Type) && handle)
            {
                MonoObject* value = ScriptAssetManager::Get().GetScriptAsset(handle, true)->GetManagedInstance();
                setter(value);
                return true;
            }
            return false;
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
                return true;
            }
            return false;
        }
        else if (typeInfo->GetType() == SerializableType::Object)
        {
            Ref<SerializableObjectInfo> objInfo = nullptr;
            Ref<SerializableTypeInfoObject> objTypeInfo =
              std::static_pointer_cast<SerializableTypeInfoObject>(typeInfo);
            if (listType == nullptr)
                UI::Property(memberInfo->m_Name.c_str());
            bool modified = false;
            if (ScriptInfoManager::Get().GetSerializableObjectInfo(objTypeInfo->m_TypeNamespace,
                                                                   objTypeInfo->m_TypeName, objInfo))
            {
                const Ref<Scene>& scene = SceneManager::GetActiveScene();
                if (getter() == nullptr)
                {
                    bool construct = objInfo->m_MonoClass->GetMethod(".ctor", 0) != nullptr;
                    setter(objInfo->m_MonoClass->CreateInstance(construct));
                    modified = true;
                }
                if (objTypeInfo->m_Flags.IsSet(ScriptFieldFlagBits::Inspectable))
                    return modified || DrawObjectInspector(objInfo, getter(), setter, depth + 1);
            }
            // ImGui::NextColumn();
        }
    }

    // This here does all the work. DrawObjectInspector is called with the MonoObject of the current class instance and
    // draws it all.
    bool ScriptInspector::DrawObjectInspector(const Ref<SerializableObjectInfo>& objectInfo, MonoObject* instance,
                                              std::function<void(void*)> setter, int depth)
    {
        bool totalModified = false;
        for (auto kv : objectInfo->m_Fields)
        {
            const Ref<SerializableMemberInfo>& memberInfo = kv.second;
            if (!memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Inspectable))
                continue;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + depth * 25);
            MonoObject* val = memberInfo->GetValue(instance);
            auto valueGetter = [&]() { return val; };
            auto valueSetter = [&](void* value) { memberInfo->SetValue(instance, value); };
            auto objectSetter = [=](void* obj) { setter((MonoObject*)obj); };
            bool modified = DrawFieldInspector(memberInfo, memberInfo->m_Name.c_str(), valueGetter, valueSetter);
            totalModified |= modified;
            if (modified && objectInfo->m_TypeInfo->m_ValueType)
            {
                if (setter)
                    setter(MonoUtils::Unbox(instance));
            }
        }
        return totalModified;
    }
} // namespace Crowny
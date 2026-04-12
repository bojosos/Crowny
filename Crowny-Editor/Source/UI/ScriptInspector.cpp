#include "cwepch.h"

#include "UI/Properties.h"
#include "UI/ScriptInspector.h"

#include "Crowny/Scripting/Mono/MonoArray.h"
#include "Crowny/Scripting/Mono/MonoMethod.h"
#include "Crowny/Scripting/Mono/MonoProperty.h"
#include "Crowny/Scripting/ScriptAssetManager.h"
#include "Crowny/Scripting/ScriptInfoManager.h"

#include <imgui.h>

namespace Crowny
{
    // -------------------------------------------------------------------------
    // Numeric field template: handles I8, U8, I16, U16, I32, U32, I64, U64
    // -------------------------------------------------------------------------
    template <typename T, ImGuiDataType_ DataType> static bool DrawNumericField(const char* label, const FieldContext& ctx, T typeMin, T typeMax)
    {
        void* fieldValue = MonoUtils::Unbox(ctx.Getter());
        T value = *(T*)fieldValue;

        // Determine range from attribute (if any)
        T rangeMin = typeMin;
        T rangeMax = typeMax;
        bool displayAsSlider = false;
        bool hasRange = ctx.MemberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Range);

        if (hasRange)
        {
            MonoClass* rangeClass = ScriptInfoManager::Get().GetBuiltinClasses().RangeAttribute;
            MonoObject* rangeAttr = ctx.MemberInfo->GetAttribute(rangeClass);
            float minF = 0.0f, maxF = 0.0f;
            rangeClass->GetField("min")->Get(rangeAttr, &minF);
            rangeClass->GetField("max")->Get(rangeAttr, &maxF);
            rangeClass->GetField("slider")->Get(rangeAttr, &displayAsSlider);
            displayAsSlider = false;

            // Clamp float range to the representable range of this integer type
            int64_t minInt = (int64_t)minF;
            int64_t maxInt = (int64_t)maxF;
            rangeMin = (T)glm::clamp((long long)minInt, (long long)typeMin, (long long)typeMax);
            rangeMax = (T)glm::clamp((long long)maxInt, (long long)typeMin, (long long)typeMax);
        }

        bool change = false;
        if (!hasRange)
            change = UI::Property(label, value, rangeMin, rangeMax);
        else if (displayAsSlider)
            change = ImGui::SliderScalar("##slider", DataType, &value, &rangeMin, &rangeMax, "%d");
        else
            change = UI::Property(label, value, rangeMin, rangeMax);

        if (change)
        {
            // Clamp using a wider type to avoid overflow during comparison
            if constexpr (std::is_signed_v<T>)
                value = (T)glm::clamp((int64_t)value, (int64_t)rangeMin, (int64_t)rangeMax);
            else
                value = (T)glm::clamp((uint64_t)value, (uint64_t)rangeMin, (uint64_t)rangeMax);
            ctx.Setter(&value);
            return true;
        }
        return false;
    }

    // -------------------------------------------------------------------------
    // String field: filepath / multiline / normal variants
    // -------------------------------------------------------------------------
    bool ScriptInspector::DrawStringField(const char* label, const FieldContext& ctx)
    {
        MonoString* value = (MonoString*)ctx.Getter();
        if (ctx.MemberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Filepath))
        {
            MonoClass* filepathClass = ScriptInfoManager::Get().GetBuiltinClasses().FilepathAttribute;
            MonoObject* filepathAttr = ctx.MemberInfo->GetAttribute(filepathClass);
            FileDialogType dialogType = FileDialogType::OpenFile;
            filepathClass->GetField("type")->Get(filepathAttr, &dialogType);

            if (value != nullptr && MonoUtils::GetClass((MonoObject*)value) != MonoUtils::GetStringClass() &&
                (dialogType == FileDialogType::Multiselect))
            {
                CW_ENGINE_ERROR("Filedialog attribute with multiselect can only be used on a list of strings");
                return false;
            }
            String stringValue = value != nullptr ? MonoUtils::FromMonoString(value) : "";
            if (UI::PropertyFilepath(label, dialogType, stringValue))
            {
                ctx.Setter(MonoUtils::ToMonoString(stringValue));
                return true;
            }
        }
        else if (ctx.MemberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Multiline))
        {
            String stringValue = value != nullptr ? MonoUtils::FromMonoString(value) : "";
            if (UI::PropertyMultiline(label, stringValue))
            {
                ctx.Setter(MonoUtils::ToMonoString(stringValue));
                return true;
            }
        }
        else
        {
            String stringValue = value != nullptr ? MonoUtils::FromMonoString(value) : "";
            if (UI::Property(label, stringValue))
            {
                ctx.Setter(MonoUtils::ToMonoString(stringValue));
                return true;
            }
        }
        return false;
    }

    // -------------------------------------------------------------------------
    // Matrix4 field: read matrix, edit 4 rows, call setter once
    // -------------------------------------------------------------------------
    bool ScriptInspector::DrawMatrix4Field(const char* label, const FieldContext& ctx)
    {
        void* fieldValue = MonoUtils::Unbox(ctx.Getter());
        glm::mat4 value = *(glm::mat4*)fieldValue;
        glm::vec4 rows[4] = {
            { value[0][0], value[1][0], value[2][0], value[3][0] },
            { value[0][1], value[1][1], value[2][1], value[3][1] },
            { value[0][2], value[1][2], value[2][2], value[3][2] },
            { value[0][3], value[1][3], value[2][3], value[3][3] },
        };

        static const char* rowLabels[4] = { "Row 1", "Row 2", "Row 3", "Row 4" };
        bool modified = false;

        UI::Property(ctx.MemberInfo->m_Name.c_str());
        for (int r = 0; r < 4; r++)
        {
            UI::ShiftCursorX(25.0f);
            if (UI::Property(rowLabels[r], rows[r]))
                modified = true;
        }

        if (modified)
        {
            for (int r = 0; r < 4; r++)
            {
                value[0][r] = rows[r][0];
                value[1][r] = rows[r][1];
                value[2][r] = rows[r][2];
                value[3][r] = rows[r][3];
            }
            ctx.Setter(&value);
        }
        return modified;
    }

    // -------------------------------------------------------------------------
    // List inspector
    // -------------------------------------------------------------------------
    // Note: setter is needed for null lists, since they are created only if a new element is added
    bool ScriptInspector::DrawListInspector(MonoObject* listObject, const FieldContext& ctx)
    {
        const Ref<SerializableTypeInfoList>& listInfo = std::static_pointer_cast<SerializableTypeInfoList>(ctx.MemberInfo->m_TypeInfo);
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
        if (UI::PropertyInput(ctx.MemberInfo->m_Name.c_str(), newLength))
        {
            if (listObject == nullptr && newLength != 0) // If not user initialized, initialize ourself
            {
                MonoClass* klass =
                  MonoManager::Get().FindClass(ctx.MemberInfo->m_TypeInfo->GetMonoClass()); // So this somehow works without having to bind parameters
                listObject = klass->CreateInstance(true);
                ctx.Setter(listObject);
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
            ImGui::SetCursorPosX(ImGui::GetCursorPos().x + (ctx.Depth + 1) * 25);
            auto getter = [itemProp, i, listObject]() { return itemProp->GetIndexed(listObject, i); };
            auto setter = [itemProp, i, listObject](void* value) { return itemProp->SetIndexed(listObject, i, value); };
            FieldContext elemCtx;
            elemCtx.MemberInfo = ctx.MemberInfo;
            elemCtx.Getter = getter;
            elemCtx.Setter = setter;
            elemCtx.OverrideTypeInfo = listInfo->m_ElementType;
            elemCtx.Depth = ctx.Depth + 1;
            modified |= DrawFieldInspector(std::to_string(i).c_str(), elemCtx);
            if (!ctx.MemberInfo->m_Tooltip.empty())
            {
                ImGui::BeginTooltip();
                ImGui::Text("%s", ctx.MemberInfo->m_Tooltip.c_str());
                ImGui::EndTooltip();
            }
        }
        return modified;
    }

    // -------------------------------------------------------------------------
    // Dictionary inspector (static state removed)
    // -------------------------------------------------------------------------
    bool ScriptInspector::DrawDictionaryInspector(MonoObject* dictObject, const FieldContext& ctx)
    {
        ImGui::PushID(ctx.Depth);
        const Ref<SerializableTypeInfoDictionary>& dictInfo = std::static_pointer_cast<SerializableTypeInfoDictionary>(ctx.MemberInfo->m_TypeInfo);
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
            ImGui::SetCursorPosX(ImGui::GetCursorPos().x + (ctx.Depth + 1) * 25);
            auto getterValue = [&]() {
                if (MonoManager::Get().FindClass(values.GetElementClass())->IsValueType())
                    return MonoUtils::Box(values.GetElementClass(), values.GetRaw(i, values.ElementSize()));
                return values.Get<MonoObject*>(i);
            };
            auto setterValue = [isKeyString, i, dictObject, addMethod, removeMethod, dictInfo, keys, params](void* value) mutable {
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

            FieldContext elemCtx;
            elemCtx.MemberInfo = ctx.MemberInfo;
            elemCtx.Getter = getterValue;
            elemCtx.Setter = setterValue;
            elemCtx.OverrideTypeInfo = dictInfo->m_ValueType;
            elemCtx.Depth = ctx.Depth + 1;

            if (isKeyString)
            {
                keyString = keys.Get<String>(i);
                modified |= DrawFieldInspector(keyString.c_str(), elemCtx);
                if (!ctx.MemberInfo->m_Tooltip.empty())
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", ctx.MemberInfo->m_Tooltip.c_str());
                    ImGui::EndTooltip();
                }
                UI::Underline(true);
                params[0] = MonoUtils::ToMonoString(keyString);
            }
            else if (isKeyInt)
            {
                keyInt = keys.Get<uint32_t>(i);
                modified |= DrawFieldInspector(std::to_string(keyInt).c_str(), elemCtx);
                if (!ctx.MemberInfo->m_Tooltip.empty())
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", ctx.MemberInfo->m_Tooltip.c_str());
                    ImGui::EndTooltip();
                }

                UI::Underline(true);
                params[0] = &keyInt;
            }
            if (ImGui::Button(UI::GenerateLabelID("-")))
                removeMethod->Invoke(dictObject, params);
            ImGui::NextColumn();
        }
        if (isKeyInt)
        {
            int32_t newKeyInt = 0;
            String newValueString;
            ImGui::PushItemWidth(-1);
            UI::PropertyDictionary(newKeyInt, newValueString);
            params[0] = &newKeyInt;
            MonoObject* containsObject = containsKey->Invoke(dictObject, params);
            bool contains = *(bool*)MonoUtils::Unbox(containsObject);
            ImGui::BeginDisabled(contains);
            if (ImGui::Button(UI::GenerateLabelID("Add")))
            {
                params[1] = MonoUtils::ToMonoString(newValueString);
                addMethod->Invoke(dictObject, params);
            }
            ImGui::EndDisabled();
        }
        else if (isKeyString)
        {
            String newKeyString;
            int newValueInt = 0;
            ImGui::PushItemWidth(-1);
            UI::PropertyDictionary(newKeyString, newValueInt);
            params[0] = MonoUtils::ToMonoString(newKeyString);
            MonoObject* containsObject = containsKey->Invoke(dictObject, params);
            bool contains = *(bool*)MonoUtils::Unbox(containsObject);
            ImGui::BeginDisabled(contains);
            if (ImGui::Button(UI::GenerateLabelID("Add")))
            {
                params[1] = &newValueInt;
                addMethod->Invoke(dictObject, params);
            }
            ImGui::EndDisabled();
        }
        ImGui::Columns(2);
        ImGui::PopID();
        return false;
    }

    // -------------------------------------------------------------------------
    // Primitive inspector (uses DrawNumericField template, DrawStringField, DrawMatrix4Field)
    // -------------------------------------------------------------------------
    bool ScriptInspector::DrawPrimitiveInspector(const char* label, const FieldContext& ctx)
    {
        const Ref<SerializableTypeInfo>& typeInfo = ctx.GetTypeInfo();
        const Ref<SerializableTypeInfoPrimitive>& primitive = std::static_pointer_cast<SerializableTypeInfoPrimitive>(typeInfo);

        if (primitive->m_Type == ScriptPrimitiveType::String)
            return DrawStringField(label, ctx);

        if (primitive->m_Type == ScriptPrimitiveType::Matrix4)
            return DrawMatrix4Field(label, ctx);

        void* fieldValue = MonoUtils::Unbox(ctx.Getter());

        switch (primitive->m_Type)
        {
        case ScriptPrimitiveType::Bool: {
            bool value = *(bool*)fieldValue;
            if (ctx.MemberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Dropdown))
            {
                if (UI::PropertyDropdown(label, { "False", "True" }, value))
                {
                    ctx.Setter(&value);
                    return true;
                }
            }
            else if (UI::Property(label, value))
            {
                ctx.Setter(&value);
                return true;
            }
            return false;
        }
        case ScriptPrimitiveType::Double: {
            float minValue = -FLT_MAX, maxValue = FLT_MAX;
            bool displayAsSlider = false;
            if (ctx.MemberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Range))
            {
                MonoClass* rangeClass = ScriptInfoManager::Get().GetBuiltinClasses().RangeAttribute;
                MonoObject* rangeAttr = ctx.MemberInfo->GetAttribute(rangeClass);
                rangeClass->GetField("min")->Get(rangeAttr, &minValue);
                rangeClass->GetField("max")->Get(rangeAttr, &maxValue);
                rangeClass->GetField("slider")->Get(rangeAttr, &displayAsSlider);
                displayAsSlider = false;
            }
            float value = (float)*(double*)fieldValue;
            if (UIUtils::DrawFloatControl(label, value, minValue, maxValue, displayAsSlider))
            {
                double val = value;
                ctx.Setter(&val);
                return true;
            }
            return false;
        }
        case ScriptPrimitiveType::Float: {
            float minValue = -FLT_MAX, maxValue = FLT_MAX;
            bool displayAsSlider = false;
            if (ctx.MemberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Range))
            {
                MonoClass* rangeClass = ScriptInfoManager::Get().GetBuiltinClasses().RangeAttribute;
                MonoObject* rangeAttr = ctx.MemberInfo->GetAttribute(rangeClass);
                rangeClass->GetField("min")->Get(rangeAttr, &minValue);
                rangeClass->GetField("max")->Get(rangeAttr, &maxValue);
                rangeClass->GetField("slider")->Get(rangeAttr, &displayAsSlider);
                displayAsSlider = false;
            }
            float value = *(float*)fieldValue;
            if (UIUtils::DrawFloatControl(label, value, minValue, maxValue, displayAsSlider))
            {
                ctx.Setter(&value);
                return true;
            }
            return false;
        }
        case ScriptPrimitiveType::Char: {
            char c = *(char*)fieldValue;
            if (UI::Property(label, c))
            {
                ctx.Setter(&c);
                return true;
            }
            return false;
        }
        case ScriptPrimitiveType::I8:
            return DrawNumericField<int8_t, ImGuiDataType_S8>(label, ctx, INT8_MIN, INT8_MAX);
        case ScriptPrimitiveType::U8:
            return DrawNumericField<uint8_t, ImGuiDataType_U8>(label, ctx, 0, UINT8_MAX);
        case ScriptPrimitiveType::I16:
            return DrawNumericField<int16_t, ImGuiDataType_S16>(label, ctx, INT16_MIN, INT16_MAX);
        case ScriptPrimitiveType::U16:
            return DrawNumericField<uint16_t, ImGuiDataType_U16>(label, ctx, 0, UINT16_MAX);
        case ScriptPrimitiveType::I32:
            return DrawNumericField<int32_t, ImGuiDataType_S32>(label, ctx, INT32_MIN, INT32_MAX);
        case ScriptPrimitiveType::U32:
            return DrawNumericField<uint32_t, ImGuiDataType_U32>(label, ctx, 0, UINT32_MAX);
        case ScriptPrimitiveType::I64:
            return DrawNumericField<int64_t, ImGuiDataType_S64>(label, ctx, INT64_MIN, INT64_MAX);
        case ScriptPrimitiveType::U64:
            return DrawNumericField<uint64_t, ImGuiDataType_U64>(label, ctx, 0, UINT64_MAX);
        case ScriptPrimitiveType::Vector2: {
            glm::vec2 value = *(glm::vec2*)fieldValue;
            if (UI::Property(label, value))
            {
                ctx.Setter(&value);
                return true;
            }
            return false;
        }
        case ScriptPrimitiveType::Vector3: {
            glm::vec3 value = *(glm::vec3*)fieldValue;
            if (UI::Property(label, value))
            {
                ctx.Setter(&value);
                return true;
            }
            return false;
        }
        case ScriptPrimitiveType::Vector4: {
            glm::vec4 value = *(glm::vec4*)fieldValue;
            if (UI::Property(label, value))
            {
                ctx.Setter(&value);
                return true;
            }
            return false;
        }
        case ScriptPrimitiveType::Color: {
            glm::vec4 value = *(glm::vec4*)fieldValue;
            ImGuiColorEditFlags flags = ImGuiColorEditFlags_AlphaPreview;
            if (ctx.MemberInfo->m_Flags.IsSet(ScriptFieldFlagBits::HDR))
                flags |= ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float;
            if (ctx.MemberInfo->m_Flags.IsSet(ScriptFieldFlagBits::NoAlpha))
                flags |= ImGuiColorEditFlags_NoAlpha;
            if (UI::PropertyColor(label, value, flags))
            {
                ctx.Setter(&value);
                return true;
            }
            return false;
        }
        default:
            break;
        }
        return false;
    }

    // -------------------------------------------------------------------------
    // Enum inspector
    // -------------------------------------------------------------------------
    bool ScriptInspector::DrawEnumInspector(const FieldContext& ctx)
    {
        const Ref<SerializableTypeInfo>& typeInfo = ctx.GetTypeInfo();
        const Ref<SerializableTypeInfoEnum>& enumInfo = std::static_pointer_cast<SerializableTypeInfoEnum>(typeInfo);

        int32_t value = *(int32_t*)MonoUtils::Unbox(ctx.Getter()); // maybe here I would have to check the underlying type.....
        if (value >= enumInfo->m_EnumNames.size())                 // Maybe clamp the value here?
        {
            ImGui::NextColumn();
            return false;
        }
        if (ctx.MemberInfo->m_Flags.IsSet(ScriptFieldFlagBits::EnumQuickTabs))
        {
            if (UI::QuickTabs(ctx.MemberInfo->m_Name.c_str(), enumInfo->m_EnumNames, enumInfo->m_EnumValues, value))
            {
                ctx.Setter(&value);
                return true;
            }
        }
        else
        {
            if (value < enumInfo->m_EnumNames.size() && UI::PropertyDropdown(ctx.MemberInfo->m_Name.c_str(), enumInfo->m_EnumNames, value))
            {
                ctx.Setter(&value);
                return true;
            }
        }
        return false;
    }

    // -------------------------------------------------------------------------
    // Field inspector (dispatcher)
    // -------------------------------------------------------------------------
    bool ScriptInspector::DrawFieldInspector(const char* label, const FieldContext& ctx)
    {
        UI::ScopedDisable disabled(ctx.MemberInfo->m_Flags.IsSet(ScriptFieldFlagBits::ReadOnly));
        const Ref<SerializableTypeInfo>& typeInfo = ctx.GetTypeInfo();

        if (typeInfo->GetType() == SerializableType::Enum)
            return DrawEnumInspector(ctx);
        else if (typeInfo->GetType() == SerializableType::Primitive)
            return DrawPrimitiveInspector(label, ctx);
        else if (typeInfo->GetType() == SerializableType::Array)
        {
            MonoArray* monoAr = (MonoArray*)ctx.Getter();
            if (monoAr != nullptr)
            {
                ScriptArray ar = ScriptArray(monoAr);
                uint32_t size = ar.Size();
                if (UI::PropertyInput(label, size))
                {
                    return true;
                    ar.Resize(size);
                    ctx.Setter(ar.GetInternal());
                }
            }
            return false;
        }
        else if (typeInfo->GetType() == SerializableType::List)
            return DrawListInspector(ctx.Getter(), ctx);
        else if (typeInfo->GetType() == SerializableType::Dictionary)
            return DrawDictionaryInspector(ctx.Getter(), ctx);
        else if (typeInfo->GetType() == SerializableType::Asset)
        {
            Ref<SerializableTypeInfoAsset> assetInfo = std::static_pointer_cast<SerializableTypeInfoAsset>(typeInfo);
            ScriptAsset* scriptAsset = ScriptAsset::ToNative(ctx.Getter());
            AssetHandle<Asset> handle;
            if (scriptAsset != nullptr)
                handle = scriptAsset->GetGenericHandle();
            if (UIUtils::AssetReference(ctx.MemberInfo->m_Name.c_str(), handle, assetInfo->Type) && handle)
            {
                MonoObject* value = ScriptAssetManager::Get().GetScriptAsset(handle, true)->GetManagedInstance();
                ctx.Setter(value);
                return true;
            }
            return false;
        }
        else if (typeInfo->GetType() == SerializableType::Entity)
        {
            ScriptEntity* scriptEntity = ScriptEntity::ToNative(ctx.Getter());

            Entity entity = { entt::null, nullptr };
            if (scriptEntity != nullptr)
                entity = scriptEntity->GetNativeEntity();
            if (UIUtils::EntityReference(ctx.MemberInfo->m_Name, entity))
            {
                if (entity)
                {
                    MonoObject* value = ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(entity)->GetManagedInstance();
                    ctx.Setter(value);
                }
                else
                    ctx.Setter(nullptr);
                return true;
            }
            return false;
        }
        else if (typeInfo->GetType() == SerializableType::Object)
        {
            Ref<SerializableObjectInfo> objInfo = nullptr;
            Ref<SerializableTypeInfoObject> objTypeInfo = std::static_pointer_cast<SerializableTypeInfoObject>(typeInfo);
            if (!ctx.OverrideTypeInfo)
                UI::Property(ctx.MemberInfo->m_Name.c_str());
            bool modified = false;
            if (ScriptInfoManager::Get().GetSerializableObjectInfo(objTypeInfo->m_TypeNamespace, objTypeInfo->m_TypeName, objInfo))
            {
                const Ref<Scene>& scene = gSceneManager->GetActiveScene();
                if (ctx.Getter() == nullptr)
                {
                    bool construct = objInfo->m_MonoClass->GetMethod(".ctor", 0) != nullptr;
                    ctx.Setter(objInfo->m_MonoClass->CreateInstance(construct));
                    modified = true;
                }
                if (objTypeInfo->m_Flags.IsSet(ScriptFieldFlagBits::Inspectable))
                    return modified || DrawObjectInspector(objInfo, ctx.Getter(), ctx.Setter, ctx.Depth + 1);
            }
        }
        return false;
    }

    // -------------------------------------------------------------------------
    // Object inspector (entry point)
    // -------------------------------------------------------------------------
    // This here does all the work. DrawObjectInspector is called with the MonoObject of the current class instance and
    // draws it all.
    bool ScriptInspector::DrawObjectInspector(const Ref<SerializableObjectInfo>& objectInfo, MonoObject* instance, std::function<void(void*)> setter,
                                              int depth)
    {
        bool totalModified = false;
        bool closed = false;
        for (auto kv : objectInfo->m_Fields)
        {
            const Ref<SerializableMemberInfo>& memberInfo = kv.second;
            if (!memberInfo->m_Flags.IsSet(ScriptFieldFlagBits::Inspectable))
                continue;

            UI::ShiftCursorX(depth * 25.0f);
            auto iterFind = objectInfo->m_Headers.find(kv.first);
            if (iterFind != objectInfo->m_Headers.end())
            {
                UI::ShiftCursor(10.0f, 9.0f);
                ImGui::Columns(1);
                const ScriptHeader& header = iterFind->second;
                if (header.Collapsable)
                {
                    if (UI::IsItemDisabled())
                    {
                        ImGui::EndDisabled();
                        closed = ImGui::CollapsingHeader(header.Label.c_str());
                        ImGui::BeginDisabled(true);
                    }
                    else
                        closed = ImGui::CollapsingHeader(header.Label.c_str());
                }
                else
                {
                    UI::ScopedFont font(UI::ScopedFont::Bold);
                    ImGui::TextUnformatted(header.Label.c_str());
                }
                ImGui::Columns(2);
            }

            if (!closed)
            {
                MonoObject* val = memberInfo->GetValue(instance);

                FieldContext ctx;
                ctx.MemberInfo = memberInfo;
                ctx.Getter = [&]() { return val; };
                ctx.Setter = [&](void* value) { memberInfo->SetValue(instance, value); };
                ctx.Depth = depth;

                bool modified = DrawFieldInspector(memberInfo->m_Name.c_str(), ctx);
                if (!memberInfo->m_Tooltip.empty())
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", memberInfo->m_Tooltip.c_str());
                    ImGui::EndTooltip();
                }
                totalModified |= modified;
                if (modified && objectInfo->m_TypeInfo->m_ValueType)
                {
                    if (setter)
                        setter(MonoUtils::Unbox(instance));
                }
            }
        }
        return totalModified;
    }
} // namespace Crowny

#include "cwpch.h"

#include <mono/metadata/object.h>

#include "Crowny/Scripting/Bindings/Assets/ScriptAsset.h"
#include "Crowny/Scripting/Serialization/SerializableField.h"
#include "Crowny/Scripting/Serialization/SerializableObject.h"

#include "Crowny/Scripting/Mono/MonoArray.h"
#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoMethod.h"
#include "Crowny/Scripting/Mono/MonoProperty.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"

namespace Crowny
{

    Ref<SerializableFieldData> SerializableFieldData::Create(const Ref<SerializableTypeInfo>& typeInfo, MonoObject* value, bool allowNull)
    {
        if (typeInfo->GetType() == SerializableType::Primitive || typeInfo->GetType() == SerializableType::Enum)
        {
            ScriptPrimitiveType primitiveType = ScriptPrimitiveType::I32;
            if (typeInfo->GetType() == SerializableType::Primitive)
                primitiveType = static_cast<SerializableTypeInfoPrimitive*>(typeInfo.get())->m_Type;
            else
                primitiveType = static_cast<SerializableTypeInfoEnum*>(typeInfo.get())->m_UnderlyingType;

            switch (primitiveType)
            {
            case ScriptPrimitiveType::Bool: {
                auto fieldData = CreateRef<SerializableFieldBool>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::Char: {
                auto fieldData = CreateRef<SerializableFieldChar>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::I8: {
                auto fieldData = CreateRef<SerializableFieldI8>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::U8: {
                auto fieldData = CreateRef<SerializableFieldU8>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::I16: {
                auto fieldData = CreateRef<SerializableFieldI16>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::U16: {
                auto fieldData = CreateRef<SerializableFieldU16>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::I32: {
                auto fieldData = CreateRef<SerializableFieldI32>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::U32: {
                auto fieldData = CreateRef<SerializableFieldU32>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::I64: {
                auto fieldData = CreateRef<SerializableFieldI64>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::U64: {
                auto fieldData = CreateRef<SerializableFieldU64>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::Float: {
                auto fieldData = CreateRef<SerializableFieldFloat>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::Double: {
                auto fieldData = CreateRef<SerializableFieldDouble>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::Vector2: {
                auto fieldData = CreateRef<SerializableFieldVector2>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::Vector3: {
                auto fieldData = CreateRef<SerializableFieldVector3>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::Vector4: {
                auto fieldData = CreateRef<SerializableFieldVector4>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::Color: {
                auto fieldData = CreateRef<SerializableFieldColor>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::Matrix4: {
                auto fieldData = CreateRef<SerializableFieldMatrix4>();
                if (value != nullptr)
                    std::memcpy(&fieldData->Value, MonoUtils::Unbox(value), sizeof(fieldData->Value));
                return fieldData;
            }
            case ScriptPrimitiveType::String: {
                MonoString* str = (MonoString*)value;
                auto fieldData = CreateRef<SerializableFieldString>();
                if (value != nullptr)
                    fieldData->Value = MonoUtils::FromMonoString(str);
                else
                    fieldData->Null = true;
                return fieldData;
            }
            default:
                break;
            }
        }
        else if (typeInfo->GetType() == SerializableType::Entity)
        {
            auto fieldData = CreateRef<SerializableFieldEntity>();
            if (value != nullptr)
            {
                ScriptEntity* scriptEntity = ScriptEntity::ToNative(value);
                fieldData->Value = scriptEntity->GetNativeEntity();
            }
            return fieldData;
        }
        else if (typeInfo->GetType() == SerializableType::Array)
        {
            const Ref<SerializableTypeInfoArray> arrayInfo = StaticRefCast<SerializableTypeInfoArray>(typeInfo);
            auto fieldData = CreateRef<SerializableFieldArray>();
            fieldData->ElementType = arrayInfo->m_ElementType;
            fieldData->Null = value == nullptr;
            if (value != nullptr)
            {
                MonoArray* array = reinterpret_cast<MonoArray*>(value);
                ScriptArray scriptArray(array);
                ::MonoClass* elementClass = scriptArray.GetElementClass();
                const bool valueType = mono_class_is_valuetype(elementClass) != 0;
                const uint32_t elementSize = static_cast<uint32_t>(mono_class_array_element_size(elementClass));
                fieldData->Values.reserve(scriptArray.Size());
                for (uint32_t i = 0; i < scriptArray.Size(); i++)
                {
                    MonoObject* element =
                      valueType ? mono_value_box(MonoManager::Get().GetDomain(), elementClass, mono_array_addr_with_size(array, elementSize, i))
                                : mono_array_get(array, MonoObject*, i);
                    fieldData->Values.push_back(Create(arrayInfo->m_ElementType, element, true));
                }
            }
            return fieldData;
        }
        else if (typeInfo->GetType() == SerializableType::List)
        {
            const Ref<SerializableTypeInfoList> listInfo = StaticRefCast<SerializableTypeInfoList>(typeInfo);
            auto fieldData = CreateRef<SerializableFieldList>();
            fieldData->ElementType = listInfo->m_ElementType;
            fieldData->Null = value == nullptr;
            if (value != nullptr)
            {
                MonoClass* listClass = MonoManager::Get().FindClass(MonoUtils::GetClass(value));
                MonoProperty* countProperty = listClass != nullptr ? listClass->GetProperty("Count") : nullptr;
                MonoProperty* itemProperty = listClass != nullptr ? listClass->GetProperty("Item") : nullptr;
                if (countProperty != nullptr && itemProperty != nullptr)
                {
                    MonoObject* countObject = countProperty->Get(value);
                    const uint32_t count = countObject != nullptr ? *static_cast<uint32_t*>(MonoUtils::Unbox(countObject)) : 0;
                    fieldData->Values.reserve(count);
                    for (uint32_t i = 0; i < count; i++)
                        fieldData->Values.push_back(Create(listInfo->m_ElementType, itemProperty->GetIndexed(value, i), true));
                }
            }
            return fieldData;
        }
        else if (typeInfo->GetType() == SerializableType::Dictionary)
        {
            // auto fieldData = CreateRef<SerializableFieldArray>();
            // if (value != nullptr)
            //     fieldData->Value = SerializableFieldArray::CreateFromExisting(value, arrayTypeInfo);
            // else if (!allowNull)
            // {
            //     Vector<uint32_t> sizes(arrayTypeInfo->m_Rank, 0);
            //     fieldData->Value = SerializableFieldArray::CreateNew(arrayTypeInfo, sizes);
            // }
            return nullptr;
        }
        else if (typeInfo->GetType() == SerializableType::Object)
        {
            auto fieldData = CreateRef<SerializableFieldObject>();
            if (value != nullptr)
                fieldData->Value = SerializableObject::CreateFromMonoObject(value);
            else if (!allowNull)
                fieldData->Value = SerializableObject::CreateNew(StaticRefCast<SerializableTypeInfoObject>(typeInfo));
            return fieldData;
        }
        else if (typeInfo->GetType() == SerializableType::Asset)
        {
            auto fieldData = CreateRef<SerializableFieldAsset>();
            if (value != nullptr)
            {
                ScriptAssetBase* scriptAssetBase = ScriptAsset::ToNative(value);
                // fieldData->Value = scriptAssetBase->GetGenericHandle();
            }
            return fieldData;
        }
        return nullptr;
    }

    void* SerializableFieldObject::GetValue()
    {
        m_ManagedInstance = Value != nullptr ? Value->GetManagedInstance() : nullptr;
        return &m_ManagedInstance;
    }

    void* SerializableFieldObject::GetValue(const Ref<SerializableTypeInfo>& targetType)
    {
        m_ManagedInstance = Value != nullptr ? Value->GetManagedInstance() : nullptr;
        if (m_ManagedInstance != nullptr && targetType != nullptr && targetType->GetType() == SerializableType::Object &&
            StaticRefCast<SerializableTypeInfoObject>(targetType)->m_ValueType)
            return MonoUtils::Unbox(m_ManagedInstance);
        return &m_ManagedInstance;
    }

    void SerializableFieldObject::Serialize()
    {
        if (Value != nullptr)
            Value->Serialize();
    }

    void SerializableFieldObject::Deserialize()
    {
        if (Value != nullptr)
            Value->Deserialize();
    }

    void SerializableFieldObject::SerializeYAML(YAML::Emitter& out) const
    {
        if (Value != nullptr)
            Value->SerializeYAML(out);
        else
            out << YAML::Null;
    }

    void SerializableFieldObject::DeserializeYAML(const YAML::Node& node)
    {
        if (node.IsNull())
            Value = nullptr;
        else
            Value = SerializableObject::DeserializeYAML(node);
    }

    static void SerializeCollection(Vector<Ref<SerializableFieldData>>& values)
    {
        for (const Ref<SerializableFieldData>& value : values)
        {
            if (value != nullptr)
                value->Serialize();
        }
    }

    static void DeserializeCollection(Vector<Ref<SerializableFieldData>>& values)
    {
        for (const Ref<SerializableFieldData>& value : values)
        {
            if (value != nullptr)
                value->Deserialize();
        }
    }

    void SerializableFieldArray::Serialize() { SerializeCollection(Values); }

    void SerializableFieldArray::Deserialize() { DeserializeCollection(Values); }

    void* SerializableFieldArray::GetValue(const Ref<SerializableTypeInfo>& targetType)
    {
        if (Null)
        {
            m_ManagedInstance = nullptr;
            return &m_ManagedInstance;
        }

        Ref<SerializableTypeInfo> targetElementType = ElementType;
        if (targetType != nullptr && targetType->GetType() == SerializableType::Array)
            targetElementType = StaticRefCast<SerializableTypeInfoArray>(targetType)->m_ElementType;
        ::MonoClass* elementClass = targetElementType != nullptr ? targetElementType->GetMonoClass() : nullptr;
        if (elementClass == nullptr)
        {
            m_ManagedInstance = nullptr;
            return &m_ManagedInstance;
        }

        MonoArray* array = mono_array_new(MonoManager::Get().GetDomain(), elementClass, Values.size());
        const bool valueType = mono_class_is_valuetype(elementClass) != 0;
        for (uint32_t i = 0; i < Values.size(); i++)
        {
            if (Values[i] == nullptr)
                continue;
            void* value = Values[i]->GetValue(targetElementType);
            if (valueType)
                mono_value_copy_array(array, i, value, 1);
            else
                mono_array_setref(array, i, value != nullptr ? *static_cast<MonoObject**>(value) : nullptr);
        }
        m_ManagedInstance = reinterpret_cast<MonoObject*>(array);
        return &m_ManagedInstance;
    }

    void SerializableFieldList::Serialize() { SerializeCollection(Values); }

    void SerializableFieldList::Deserialize() { DeserializeCollection(Values); }

    void* SerializableFieldList::GetValue(const Ref<SerializableTypeInfo>& targetType)
    {
        if (Null || targetType == nullptr || targetType->GetType() != SerializableType::List)
        {
            m_ManagedInstance = nullptr;
            return &m_ManagedInstance;
        }

        const Ref<SerializableTypeInfoList> listInfo = StaticRefCast<SerializableTypeInfoList>(targetType);
        MonoClass* listClass = MonoManager::Get().FindClass(listInfo->GetMonoClass());
        if (listClass == nullptr)
        {
            m_ManagedInstance = nullptr;
            return &m_ManagedInstance;
        }

        m_ManagedInstance = listClass->CreateInstance(true);
        MonoMethod* addMethod = listClass->GetMethod("Add", 1);
        if (addMethod == nullptr)
            return &m_ManagedInstance;
        for (const Ref<SerializableFieldData>& value : Values)
        {
            MonoObject* nullObject = nullptr;
            void* argument = value != nullptr ? value->GetValue(listInfo->m_ElementType) : &nullObject;
            void* parameters[1] = { argument };
            addMethod->Invoke(m_ManagedInstance, parameters);
        }
        return &m_ManagedInstance;
    }

} // namespace Crowny

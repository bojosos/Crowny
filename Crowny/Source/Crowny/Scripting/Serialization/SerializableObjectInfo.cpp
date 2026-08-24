#include "cwpch.h"

#include <mono/metadata/class.h>

#include "Crowny/Scripting/Mono/MonoProperty.h"
#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/Serialization/SerializableField.h"
#include "Crowny/Scripting/Serialization/SerializableObjectInfo.h"

namespace Crowny
{

    Ref<SerializableMemberInfo> SerializableObjectInfo::FindMatchingField(const Ref<SerializableMemberInfo>& fieldInfo,
                                                                          const Ref<SerializableTypeInfo>& fieldTypeInfo) const
    {
        const SerializableObjectInfo* objInfo = this;
        while (objInfo != nullptr)
        {
            if (objInfo->m_TypeInfo->Matches(fieldTypeInfo))
            {
                const auto findIter = objInfo->m_FieldNameToId.find(fieldInfo->m_Name);
                if (findIter != objInfo->m_FieldNameToId.end())
                {
                    const auto findIter2 = objInfo->m_Fields.find(findIter->second);
                    if (findIter2 != objInfo->m_Fields.end())
                    {
                        const Ref<SerializableMemberInfo> foundField = findIter2->second;
                        if (foundField->IsSerializable())
                        {
                            if (fieldInfo->m_TypeInfo->Matches(foundField->m_TypeInfo))
                                return foundField;
                        }
                    }
                }
                return nullptr;
            }
            if (objInfo->m_BaseClass != nullptr)
                objInfo = objInfo->m_BaseClass.get();
            else
                objInfo = nullptr;
        }
        return nullptr;
    }

    MonoObject* SerializableTypeInfoObject::GetAttribute(MonoClass* monoClass)
    {
        Ref<SerializableObjectInfo> objInfo;
        if (!ScriptInfoManager::Get().GetSerializableObjectInfo(m_TypeNamespace, m_TypeName, objInfo))
            return nullptr;
        return objInfo->m_MonoClass->GetAttribute(monoClass);
    }

    MonoObject* SerializableFieldInfo::GetAttribute(MonoClass* monoClass) { return m_Field->GetAttribute(monoClass); }

    MonoObject* SerializableFieldInfo::GetValue(MonoObject* instance) const
    {
        // void* value;
        return m_Field->GetBoxed(instance);
        // return value;
    }

    void SerializableFieldInfo::SetValue(MonoObject* instance, void* value) const { m_Field->Set(instance, value); }

    MonoObject* SerializablePropertyInfo::GetAttribute(MonoClass* monoClass) { return m_Property->GetAttribute(monoClass); }

    MonoObject* SerializablePropertyInfo::GetValue(MonoObject* instance) const { return m_Property->Get(instance); }

    void SerializablePropertyInfo::SetValue(MonoObject* instance, void* value) const { m_Property->Set(instance, value); }

    bool SerializableTypeInfoPrimitive::Matches(const Ref<SerializableTypeInfo>& typeInfo) const
    {
        // CW_ENGINE_INFO("Here Match TypeInfoPrimitive");
        if (typeInfo->GetType() == SerializableType::Primitive)
        {
            const auto primitiveTypeInfo = StaticRefCast<SerializableTypeInfoPrimitive>(typeInfo);
            return primitiveTypeInfo->m_Type == m_Type;
        }
        return false;
    }

    ::MonoClass* SerializableTypeInfoPrimitive::GetMonoClass() const
    {
        switch (m_Type)
        {
        case ScriptPrimitiveType::Bool:
            return MonoUtils::GetBoolClass();
        case ScriptPrimitiveType::Char:
            return MonoUtils::GetCharClass();
        case ScriptPrimitiveType::I8:
            return MonoUtils::GetByteClass();
        case ScriptPrimitiveType::U8:
            return MonoUtils::GetByteClass();
        case ScriptPrimitiveType::I16:
            return MonoUtils::GetI16Class();
        case ScriptPrimitiveType::U16:
            return MonoUtils::GetU16Class();
        case ScriptPrimitiveType::I32:
            return MonoUtils::GetI32Class();
        case ScriptPrimitiveType::U32:
            return MonoUtils::GetU32Class();
        case ScriptPrimitiveType::I64:
            return MonoUtils::GetI64Class();
        case ScriptPrimitiveType::U64:
            return MonoUtils::GetU64Class();
        case ScriptPrimitiveType::Float:
            return MonoUtils::GetFloatClass();
        case ScriptPrimitiveType::Double:
            return MonoUtils::GetDoubleClass();
        case ScriptPrimitiveType::String:
            return MonoUtils::GetStringClass();
        default:
            break;
        }
        return nullptr;
    }

    bool SerializableTypeInfoEnum::Matches(const Ref<SerializableTypeInfo>& typeInfo) const
    {
        if (typeInfo->GetType() == SerializableType::Enum)
        {
            // Maybe worth saving the enum names, so that if I add another in the middle of the enum, I know to change
            // the saved value
            const auto* enumTypeInfo = static_cast<SerializableTypeInfoEnum*>(typeInfo.get());
            return enumTypeInfo->m_TypeNamespace == m_TypeNamespace && enumTypeInfo->m_TypeName == m_TypeName &&
                   enumTypeInfo->m_UnderlyingType == m_UnderlyingType;
        }
        return false;
    }

    ::MonoClass* SerializableTypeInfoEnum::GetMonoClass() const
    {
        MonoClass* monoClass = MonoManager::Get().FindClass(m_TypeNamespace, m_TypeName);
        if (monoClass)
            return monoClass->GetInternalPtr();
        return nullptr;
    }

    bool SerializableTypeInfoEntity::Matches(const Ref<SerializableTypeInfo>& typeInfo) const
    {
        return typeInfo->GetType() == SerializableType::Entity;
    }

    ::MonoClass* SerializableTypeInfoEntity::GetMonoClass() const { return ScriptEntity::GetMetaData()->ScriptClass->GetInternalPtr(); }

    bool SerializableTypeInfoAsset::Matches(const Ref<SerializableTypeInfo>& typeInfo) const
    {
        return typeInfo->GetType() == SerializableType::Asset && StaticRefCast<SerializableTypeInfoAsset>(typeInfo)->Type == Type;
    }

    ::MonoClass* SerializableTypeInfoAsset::GetMonoClass() const
    {
        AssetInfo* assetInfo = ScriptInfoManager::Get().GetAssetInfo(Type);
        return assetInfo != nullptr && assetInfo->AssetClass != nullptr ? assetInfo->AssetClass->GetInternalPtr() : nullptr;
    }

    bool SerializableTypeInfoList::Matches(const Ref<SerializableTypeInfo>& typeInfo) const
    {
        if (typeInfo->GetType() != SerializableType::List)
            return false;
        const Ref<SerializableTypeInfoList> listInfo = StaticRefCast<SerializableTypeInfoList>(typeInfo);
        return m_ElementType != nullptr && listInfo->m_ElementType != nullptr && m_ElementType->Matches(listInfo->m_ElementType);
    }

    bool SerializableTypeInfoObject::Matches(const Ref<SerializableTypeInfo>& typeInfo) const
    {
        if (typeInfo->GetType() == SerializableType::Object)
        {
            const auto* objTypeInfo = static_cast<SerializableTypeInfoObject*>(typeInfo.get());
            return objTypeInfo->m_TypeNamespace == m_TypeNamespace && objTypeInfo->m_TypeName == m_TypeName &&
                   objTypeInfo->m_ValueType == m_ValueType;
        }
        return false;
    }

    ::MonoClass* SerializableTypeInfoObject::GetMonoClass() const
    {
        Ref<SerializableObjectInfo> objInfo;
        if (!ScriptInfoManager::Get().GetSerializableObjectInfo(m_TypeNamespace, m_TypeName, objInfo))
            return nullptr;
        return objInfo->m_MonoClass->GetInternalPtr();
    }

    bool SerializableTypeInfoArray::Matches(const Ref<SerializableTypeInfo>& typeInfo) const
    {
        if (typeInfo->GetType() == SerializableType::Array)
        {
            const auto* arrayTypeInfo = static_cast<SerializableTypeInfoArray*>(typeInfo.get());
            return m_ElementType != nullptr && arrayTypeInfo->m_ElementType != nullptr && m_ElementType->Matches(arrayTypeInfo->m_ElementType);
        }
        return false;
    }

    ::MonoClass* SerializableTypeInfoArray::GetMonoClass() const
    {
        ::MonoClass* elementClass = m_ElementType != nullptr ? m_ElementType->GetMonoClass() : nullptr;
        return elementClass != nullptr ? mono_array_class_get(elementClass, 1) : nullptr;
    }

    bool SerializableTypeInfoDictionary::Matches(const Ref<SerializableTypeInfo>& typeInfo) const
    {
        if (typeInfo->GetType() != SerializableType::Dictionary)
            return false;
        const Ref<SerializableTypeInfoDictionary> dictionaryInfo = StaticRefCast<SerializableTypeInfoDictionary>(typeInfo);
        return m_KeyType != nullptr && m_ValueType != nullptr && dictionaryInfo->m_KeyType != nullptr && dictionaryInfo->m_ValueType != nullptr &&
               m_KeyType->Matches(dictionaryInfo->m_KeyType) && m_ValueType->Matches(dictionaryInfo->m_ValueType);
    }

} // namespace Crowny

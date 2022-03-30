#include "cwpch.h"

#include "Crowny/Scripting/Mono/MonoUtils.h"
#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/Serialization/SerializableField.h"
#include "Crowny/Scripting/Serialization/SerializableObject.h"

namespace Crowny
{

    SerializableObject::SerializableObject(Ref<SerializableObjectInfo> objInfo, MonoObject* managedInstance)
      : m_ObjectInfo(objInfo)
    {
        m_GCHandle = MonoUtils::NewGCHandle(managedInstance, false);
    }

    SerializableObject::~SerializableObject()
    {
        if (m_GCHandle != 0)
        {
            MonoUtils::FreeGCHandle(m_GCHandle);
            m_GCHandle = 0;
        }
    }

    void SerializableObject::Serialize() // This one I will use for serializing objects inside other objects
    {
    }

    void SerializableObject::Deserialize(MonoObject* instance, const Ref<SerializableObjectInfo>& objInfo)
    {
        if (instance == nullptr)
            return;
        for (auto& fieldEntry : m_CachedData)
            fieldEntry.second->Deserialize();
        uint32_t i = 0;
        Ref<SerializableObjectInfo> type = m_ObjectInfo;
        while (type != nullptr)
        {
            for (auto& field : type->m_Fields)
            {
                if (field.second->IsSerializable())
                {
                    uint32_t fieldId = field.second->m_FieldId;
                    uint32_t typeId = field.second->m_ParentTypeId;
                    SerializableFieldKey key(typeId, fieldId);
                    Ref<SerializableMemberInfo> fieldInfo = objInfo->FindMatchingField(field.second, type->m_TypeInfo);
                    if (fieldInfo != nullptr)
                        fieldInfo->SetValue(instance, m_CachedData[key]->GetValue(/*fieldInfo->m_TypeInfo*/));
                    i++;
                }
            }
            type = type->m_BaseClass;
        }
    }

    Ref<SerializableFieldData> SerializableObject::GetFieldData(const Ref<SerializableMemberInfo>& fieldInfo) const
    {
        if (m_GCHandle != 0)
        {
            MonoObject* managedInstance = MonoUtils::GetObjectFromGCHandle(m_GCHandle);
            MonoObject* fieldValue = (MonoObject*)fieldInfo->GetValue(managedInstance);
            return SerializableFieldData::Create(fieldInfo->m_TypeInfo, fieldValue);
        }
        else
        {
            SerializableFieldKey key(fieldInfo->m_ParentTypeId, fieldInfo->m_FieldId);
            auto iterFind = m_CachedData.find(key);
            if (iterFind != m_CachedData.end())
                return iterFind->second;
            return nullptr;
        }
    }

    void SerializableObject::SetFieldData(const Ref<SerializableMemberInfo>& fieldInfo,
                                          const Ref<SerializableFieldData>& val)
    {
        if (m_GCHandle != 0)
        {
            MonoObject* managedInstance = MonoUtils::GetObjectFromGCHandle(m_GCHandle);
            fieldInfo->SetValue(managedInstance, val->GetValue(/*fieldInfo->m_TypeInfo*/));
        }
        else
        {
            SerializableFieldKey key(fieldInfo->m_ParentTypeId, fieldInfo->m_FieldId);
            m_CachedData[key] = val;
        }
    }

    Ref<SerializableObject> SerializableObject::CreateNew(const Ref<SerializableTypeInfoObject>& type)
    {
        Ref<SerializableObjectInfo> objInfo = nullptr;
        if (!ScriptInfoManager::Get().GetSerializableObjectInfo(type->m_TypeNamespace, type->m_TypeName, objInfo))
            return nullptr;
        return CreateRef<SerializableObject>(objInfo, CreateManagedInstance(type));
    }

    MonoObject* SerializableObject::CreateManagedInstance(const Ref<SerializableTypeInfoObject>& type)
    {
        Ref<SerializableObjectInfo> objInfo = nullptr;
        if (!ScriptInfoManager::Get().GetSerializableObjectInfo(type->m_TypeNamespace, type->m_TypeName, objInfo))
            return nullptr;

        const bool hasCtor = objInfo->m_MonoClass->GetMethod(".ctor", 0) != nullptr;
        return objInfo->m_MonoClass->CreateInstance(hasCtor);
    }

    Ref<SerializableObject> SerializableObject::CreateFromMonoObject(MonoObject* managedInstance)
    {
        if (managedInstance == nullptr)
            return nullptr;
        String ns, typeName;
        MonoUtils::GetClassName(managedInstance, ns, typeName);
        Ref<SerializableObjectInfo> objInfo;
        if (!ScriptInfoManager::Get().GetSerializableObjectInfo(ns, typeName, objInfo))
            return nullptr;
        return CreateRef<SerializableObject>(objInfo, managedInstance);
    }

    size_t SerializableObject::Hash::operator()(const SerializableFieldKey& x) const
    {
        size_t seed = 0;
        HashCombine(seed, (uint32_t)x.m_FieldIdx);
        HashCombine(seed, (uint32_t)x.m_TypeId);
        return seed;
    }

    bool SerializableObject::Equals::operator()(const SerializableFieldKey& l, const SerializableFieldKey& r) const
    {
        return l.m_FieldIdx == r.m_FieldIdx && l.m_TypeId == r.m_TypeId;
    }

} // namespace Crowny
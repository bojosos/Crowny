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

    SerializableObject::SerializableObject(const Ref<SerializableObjectInfo>& objInfo) : m_ObjectInfo(objInfo) {}

    SerializableObject::~SerializableObject()
    {
        if (m_GCHandle != 0)
        {
            MonoUtils::FreeGCHandle(m_GCHandle);
            m_GCHandle = 0;
        }
    }

    void SerializableObject::Serialize()
    {
        if (m_GCHandle == 0)
            return;
        m_CachedData.clear();

        Ref<SerializableObjectInfo> type = m_ObjectInfo;
        while (type != nullptr)
        {
            for (auto [id, memberInfo] : type->m_Fields)
            {
                if (memberInfo->IsSerializable())
                {
                    CW_ENGINE_INFO(memberInfo->m_Name);
                    SerializableFieldKey key(memberInfo->m_ParentTypeId, memberInfo->m_FieldId);
                    m_CachedData[key] = GetFieldData(memberInfo);
                }
            }
            type = type->m_BaseClass;
        }

        for (auto [key, field] : m_CachedData)
            field->Serialize();

        MonoUtils::FreeGCHandle(m_GCHandle);
        m_GCHandle = 0;
    }

    MonoObject* SerializableObject::Deserialize()
    {
        Ref<SerializableObjectInfo> objInfo = nullptr;
        if (!ScriptInfoManager::Get().GetSerializableObjectInfo(m_ObjectInfo->m_TypeInfo->m_TypeNamespace,
                                                                m_ObjectInfo->m_TypeInfo->m_TypeName, objInfo))
            return nullptr;
        MonoObject* managedInstance = CreateManagedInstance(objInfo->m_TypeInfo);
        Deserialize(managedInstance, objInfo);
        return managedInstance;
    }

    void SerializeTypeInfo(const Ref<SerializableMemberInfo>& field, YAML::Emitter& out)
    {
        out << YAML::BeginMap;
        if (field->m_TypeInfo->GetType() == SerializableType::Primitive)
            out << YAML::Key << "PrimitiveInfo" << YAML::Value
                << (uint32_t)std::static_pointer_cast<SerializableTypeInfoPrimitive>(field->m_TypeInfo)->m_Type;
        else if (field->m_TypeInfo->GetType() == SerializableType::Enum)
        {
            out << YAML::Key << "EnumInfo" << YAML::Value << YAML::BeginMap;
            auto enumInfo = std::static_pointer_cast<SerializableTypeInfoEnum>(field->m_TypeInfo);
            out << YAML::Key << "TypeName" << YAML::Value << enumInfo->m_TypeName;
            out << YAML::Key << "TypeNamespace" << YAML::Value << enumInfo->m_TypeNamespace;
            out << YAML::Key << "UnderlyingType" << YAML::Value << (uint32_t)enumInfo->m_UnderlyingType;

            out << YAML::EndMap;
        }
        else if (field->m_TypeInfo->GetType() == SerializableType::Object)
        {
            out << YAML::Key << "ObjectInfo" << YAML::Value << YAML::BeginMap;
            auto objTypeInfo = std::static_pointer_cast<SerializableTypeInfoObject>(field->m_TypeInfo);
            out << YAML::Key << "TypeName" << YAML::Value << objTypeInfo->m_TypeName;
            out << YAML::Key << "TypeNamespace" << YAML::Value << objTypeInfo->m_TypeNamespace;
            out << YAML::Key << "ValueType" << YAML::Value << objTypeInfo->m_ValueType;
            out << YAML::Key << "TypeId" << YAML::Value << objTypeInfo->m_TypeId;
            out << YAML::Key << "Flags" << YAML::Value << (uint32_t)objTypeInfo->m_Flags;
            // Maybe need another id here
            out << YAML::EndMap;
        }
        out << YAML::EndMap;
    }

    Ref<SerializableTypeInfo> DeserializeTypeInfo(const YAML::Node& node)
    {
        if (const YAML::Node& prim = node["PrimitiveInfo"])
        {
            Ref<SerializableTypeInfoPrimitive> primInfo = CreateRef<SerializableTypeInfoPrimitive>();
            primInfo->m_Type = (ScriptPrimitiveType)prim.as<uint32_t>();
            return primInfo;
        }
        else if (const YAML::Node& enumNode = node["EnumInfo"])
        {
            Ref<SerializableTypeInfoEnum> enumInfo = CreateRef<SerializableTypeInfoEnum>();
            enumInfo->m_UnderlyingType = (ScriptPrimitiveType)enumNode["UnderlyingType"].as<uint32_t>();
            enumInfo->m_TypeNamespace = enumNode["TypeNamespace"].as<String>();
            enumInfo->m_TypeName = enumNode["TypeName"].as<String>();
        }
        else if (const YAML::Node& obj = node["ObjectInfo"])
        {
            Ref<SerializableTypeInfoObject> objInfo = CreateRef<SerializableTypeInfoObject>();
            objInfo->m_TypeName = obj["TypeName"].as<String>();
            objInfo->m_TypeNamespace = obj["TypeNamespace"].as<String>();
            objInfo->m_ValueType = obj["ValueType"].as<bool>();
            objInfo->m_TypeId = obj["TypeId"].as<uint32_t>();
            objInfo->m_Flags = (ScriptFieldFlags)obj["Flags"].as<uint32_t>();
        }
        return nullptr;
    }

    void SerializableObject::SerializeYAML(YAML::Emitter& out) const
    {
        Ref<SerializableObjectInfo> curType = m_ObjectInfo;

        while (curType != nullptr)
        {
            auto& objTypeInfo = curType->m_TypeInfo;
            out << YAML::BeginMap;
            out << YAML::Key << "ObjectInfo" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "TypeName" << YAML::Value << objTypeInfo->m_TypeName;
            out << YAML::Key << "TypeNamespace" << YAML::Value << objTypeInfo->m_TypeNamespace;
            out << YAML::Key << "ValueType" << YAML::Value << objTypeInfo->m_ValueType;
            out << YAML::Key << "TypeId" << YAML::Value << objTypeInfo->m_TypeId;
            out << YAML::Key << "Flags" << YAML::Value << (uint32_t)objTypeInfo->m_Flags;
            // Maybe need another id here
            out << YAML::EndMap;
            // out << YAML::EndMap;

            // out << YAML::BeginMap;
            out << YAML::Key << "Fields" << YAML::Value << YAML::BeginSeq;
            for (auto [id, field] : curType->m_Fields)
            {
                if (field->IsSerializable())
                {
                    out << YAML::BeginMap;
                    out << YAML::Key << "ParentTypeId" << YAML::Value << field->m_ParentTypeId;
                    out << YAML::Key << "FieldId" << YAML::Value << field->m_FieldId;
                    out << YAML::Key << "Flags" << YAML::Value << (uint32_t)field->m_Flags;
                    out << YAML::Key << "TypeInfo" << YAML::Value;
                    SerializeTypeInfo(field, out);
                    out << YAML::Key << "Name" << YAML::Value << field->m_Name;
                    out << YAML::Key << "Value" << YAML::Value;
                    Ref<SerializableFieldData> fieldData = GetFieldData(field);
                    fieldData->SerializeYAML(out);
                    out << YAML::EndMap;
                }
            }
            out << YAML::EndSeq;
            out << YAML::EndMap;
            curType = curType->m_BaseClass;
        }
    }

    Ref<SerializableObject> SerializableObject::DeserializeYAML(const YAML::Node& node)
    {
        for (const YAML::Node& n : node)
        {
            const YAML::Node& objectInfo = n["ObjectInfo"]; // Could use deserialize type info
            const YAML::Node& fields = n["Fields"];
            Ref<SerializableTypeInfoObject> typeInfo = CreateRef<SerializableTypeInfoObject>();
            typeInfo->m_TypeName = objectInfo["TypeName"].as<String>();
            typeInfo->m_TypeNamespace = objectInfo["TypeNamespace"].as<String>();
            typeInfo->m_Flags = (ScriptFieldFlags)objectInfo["Flags"].as<uint32_t>();
            typeInfo->m_TypeId = objectInfo["TypeId"].as<uint32_t>();
            typeInfo->m_ValueType = objectInfo["ValueType"].as<bool>();

            // Ref<SerializableObject> obj = SerializableObject::CreateNew(typeInfo);
            Ref<SerializableObjectInfo> objInfo = nullptr;
            // This is wrong, obj info should be deserialized, not loaded like this
            if (!ScriptInfoManager::Get().GetSerializableObjectInfo(typeInfo->m_TypeNamespace, typeInfo->m_TypeName,
                                                                    objInfo))
                return nullptr;
            Ref<SerializableObject> obj = CreateRef<SerializableObject>(objInfo);

            for (const YAML::Node& field : fields)
            {
                const YAML::Node& typeInfoNode = field["TypeInfo"];
                Ref<SerializableTypeInfo> typeInfo = DeserializeTypeInfo(typeInfoNode);
                Ref<SerializableMemberInfo> memberInfo = CreateRef<SerializableFieldInfo>();
                memberInfo->m_TypeInfo = typeInfo;
                memberInfo->m_FieldId = field["FieldId"].as<uint32_t>();
                memberInfo->m_Flags = (ScriptFieldFlags)field["Flags"].as<uint32_t>();
                memberInfo->m_ParentTypeId = field["ParentTypeId"].as<uint32_t>();
                memberInfo->m_Name = field["Name"].as<String>();
                SerializableFieldKey key(memberInfo->m_ParentTypeId, memberInfo->m_FieldId);
                Ref<SerializableFieldData> data;

                switch (typeInfo->GetType())
                {
                case (SerializableType::Primitive): {
                    auto prim = std::static_pointer_cast<SerializableTypeInfoPrimitive>(typeInfo);
                    switch (prim->m_Type)
                    {
                    case (ScriptPrimitiveType::Bool):
                        data = CreateRef<SerializableFieldBool>();
                        break;
                    case (ScriptPrimitiveType::Char):
                        data = CreateRef<SerializableFieldChar>();
                        break;
                    case (ScriptPrimitiveType::I8):
                        data = CreateRef<SerializableFieldI8>();
                        break;
                    case (ScriptPrimitiveType::U8):
                        data = CreateRef<SerializableFieldU8>();
                        break;
                    case (ScriptPrimitiveType::I16):
                        data = CreateRef<SerializableFieldI16>();
                        break;
                    case (ScriptPrimitiveType::U16):
                        data = CreateRef<SerializableFieldU16>();
                        break;
                    case (ScriptPrimitiveType::I32):
                        data = CreateRef<SerializableFieldI32>();
                        break;
                    case (ScriptPrimitiveType::U32):
                        data = CreateRef<SerializableFieldU32>();
                        break;
                    case (ScriptPrimitiveType::I64):
                        data = CreateRef<SerializableFieldI64>();
                        break;
                    case (ScriptPrimitiveType::U64):
                        data = CreateRef<SerializableFieldU64>();
                        break;
                    case (ScriptPrimitiveType::Float):
                        data = CreateRef<SerializableFieldFloat>();
                        break;
                    case (ScriptPrimitiveType::Double):
                        data = CreateRef<SerializableFieldDouble>();
                        break;
                    case (ScriptPrimitiveType::String):
                        data = CreateRef<SerializableFieldString>();
                        break;
                    }
                    break;
                }
                case (SerializableType::Object): {
                    auto obj = std::static_pointer_cast<SerializableTypeInfoObject>(typeInfo);
                    data = CreateRef<SerializableFieldObject>();
                    break;
                }
                case (SerializableType::Entity): {
                    auto obj = std::static_pointer_cast<SerializableTypeInfoEntity>(typeInfo);
                    data = CreateRef<SerializableFieldEntity>();
                    break;
                }
                }
                data->DeserializeYAML(field["Value"]);
                obj->m_CachedData[key] = data;
            }
            return obj;
            // MonoObject* instance = obj->Deserialize();
        }
        return nullptr;
    }

    void SerializableObject::Deserialize(MonoObject* instance, const Ref<SerializableObjectInfo>& objInfo)
    {
        if (instance == nullptr)
            return;
        for (auto& fieldEntry : m_CachedData)
            fieldEntry.second->Deserialize();

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
                    CW_ENGINE_ASSERT(fieldInfo !=
                                     nullptr); // TODO: Remove this, it will cause crashes when recompiling C#
                    if (fieldInfo != nullptr)
                    {
                        // CW_ENGINE_INFO(fieldInfo->GetValue(instance));
                        fieldInfo->SetValue(instance, m_CachedData[key]->GetValue(/*fieldInfo->m_TypeInfo*/));
                        // CW_ENGINE_INFO(*(int*)m_CachedData[key]->GetValue());
                        // CW_ENGINE_INFO(*(int*)MonoUtils::Unbox(fieldInfo->GetValue(instance)));
                    }
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
            return SerializableFieldData::Create(fieldInfo->m_TypeInfo, fieldValue, false);
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
        if (val == nullptr)
            CW_ENGINE_INFO("Here");
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

    MonoObject* SerializableObject::GetManagedInstance() const
    {
        if (m_GCHandle != 0)
            return MonoUtils::GetObjectFromGCHandle(m_GCHandle);
        return nullptr;
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
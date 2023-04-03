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
        {
            ScriptPrimitiveType primitiveType =
              std::static_pointer_cast<SerializableTypeInfoPrimitive>(field->m_TypeInfo)->m_Type;

            SerializeEnumYAML(out, "PrimitiveInfo", primitiveType);
        }
        else if (field->m_TypeInfo->GetType() == SerializableType::Enum)
        {
            BeginYAMLMap(out, "EnumInfo");

            auto enumInfo = std::static_pointer_cast<SerializableTypeInfoEnum>(field->m_TypeInfo);
            SerializeValueYAML(out, "TypeName", enumInfo->m_TypeName);
            SerializeValueYAML(out, "TypeNamespace", enumInfo->m_TypeNamespace);
            SerializeEnumYAML(out, "UnderlyingType", enumInfo->m_UnderlyingType);

            EndYAMLMap(out, "EnumInfo");
        }
        else if (field->m_TypeInfo->GetType() == SerializableType::Object)
        {
            BeginYAMLMap(out, "ObjectInfo");

            auto objTypeInfo = std::static_pointer_cast<SerializableTypeInfoObject>(field->m_TypeInfo);
            SerializeValueYAML(out, "TypeName", objTypeInfo->m_TypeName);
            SerializeValueYAML(out, "TypeNamespace", objTypeInfo->m_TypeNamespace);
            SerializeValueYAML(out, "ValueType", objTypeInfo->m_ValueType);
            SerializeValueYAML(out, "TypeId", objTypeInfo->m_TypeId);
            SerializeFlagsYAML(out, "Flags", objTypeInfo->m_Flags);
            // Maybe need another id here

            EndYAMLMap(out, "ObjectInfo");
        }
        else if (field->m_TypeInfo->GetType() == SerializableType::Entity)
            SerializeValueYAML(out, "EntityInfo", YAML::Null);

        out << YAML::EndMap;
    }

    Ref<SerializableTypeInfo> DeserializeTypeInfo(const YAML::Node& node)
    {
        if (const YAML::Node& primitiveInfoNode = node["PrimitiveInfo"])
        {
            Ref<SerializableTypeInfoPrimitive> primitiveInfo = CreateRef<SerializableTypeInfoPrimitive>();
            primitiveInfo->m_Type =
              (ScriptPrimitiveType)primitiveInfoNode.as<int32_t>((int32_t)ScriptPrimitiveType::I32);
            if ((int32_t)primitiveInfo->m_Type < 0 || primitiveInfo->m_Type >= ScriptPrimitiveType::Count)
            {
                CW_ENGINE_WARN("Script primitive type is invalid");
                primitiveInfo->m_Type = ScriptPrimitiveType::I8;
            }
            return primitiveInfo;
        }
        else if (const YAML::Node& enumInfoNode = node["EnumInfo"])
        {
            Ref<SerializableTypeInfoEnum> enumInfo = CreateRef<SerializableTypeInfoEnum>();
            DeserializeEnumYAML(enumInfoNode, "UnderlyingType", enumInfo->m_UnderlyingType, ScriptPrimitiveType::I32);
            DeserializeValueYAML(enumInfoNode, "TypeNamespace", enumInfo->m_TypeNamespace, String());
            DeserializeValueYAML(enumInfoNode, "TypeName", enumInfo->m_TypeName, String());
            return enumInfo;
        }
        else if (const YAML::Node& objectInfoNode = node["ObjectInfo"])
        {
            Ref<SerializableTypeInfoObject> objectInfo = CreateRef<SerializableTypeInfoObject>();
            DeserializeValueYAML(objectInfoNode, "TypeName", objectInfo->m_TypeName, String());
            DeserializeValueYAML(objectInfoNode, "TypeNamespace", objectInfo->m_TypeNamespace, String());
            DeserializeValueYAML(objectInfoNode, "ValueType", objectInfo->m_ValueType, false);
            DeserializeValueYAML(objectInfoNode, "TypeId", objectInfo->m_TypeId, 0U);
            DeserializeFlagsYAML(objectInfoNode, "Flags", objectInfo->m_Flags, ScriptFieldFlagBits::None);
            return objectInfo;
        }
        else if (const YAML::Node& entityInfoNode = node["EntityInfo"])
        {
            Ref<SerializableTypeInfoEntity> entityInfo = CreateRef<SerializableTypeInfoEntity>();
            return entityInfo;
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

            BeginYAMLMap(out, "ObjectInfo");

            SerializeValueYAML(out, "TypeName", objTypeInfo->m_TypeName);
            SerializeValueYAML(out, "TypeNamespace", objTypeInfo->m_TypeNamespace);
            SerializeValueYAML(out, "ValueType", objTypeInfo->m_ValueType);
            SerializeValueYAML(out, "TypeId", objTypeInfo->m_TypeId);
            SerializeFlagsYAML(out, "Flags", objTypeInfo->m_Flags);
            // Maybe need another id here

            EndYAMLMap(out, "ObjectInfo");

            // out << YAML::BeginMap;
            SerializeValueYAML(out, "Fields", YAML::BeginSeq);
            for (auto [id, field] : curType->m_Fields)
            {
                if (field->IsSerializable())
                {
                    out << YAML::BeginMap;

                    SerializeValueYAML(out, "ParentTypeId", field->m_ParentTypeId);
                    SerializeValueYAML(out, "FieldId", field->m_FieldId);
                    SerializeFlagsYAML(out, "Flags", field->m_Flags);
                    out << YAML::Key << "TypeInfo" << YAML::Value;
                    SerializeTypeInfo(field, out);
                    SerializeValueYAML(out, "Name", field->m_Name);
                    out << YAML::Key << "Value" << YAML::Value;
                    Ref<SerializableFieldData> fieldData = GetFieldData(field);
                    fieldData->SerializeYAML(out);

                    out << YAML::EndMap;
                }
            }
            EndYAMLSeq(out);
            out << YAML::EndMap;
            curType = curType->m_BaseClass;
        }
    }

    Ref<SerializableObject> SerializableObject::DeserializeYAML(const YAML::Node& objectInfosNode)
    {
        Ref<SerializableObjectInfo> lastObjectInfo = nullptr;

        Ref<SerializableObject> result = CreateRef<SerializableObject>();
        // Each node is an object info. First one is the current object's and the ones after are the base classes.
        for (const YAML::Node& objectInfoParentNode : objectInfosNode)
        {
            const YAML::Node& objectInfoNode = objectInfoParentNode["ObjectInfo"];
            const YAML::Node& fields = objectInfoParentNode["Fields"];

            Ref<SerializableTypeInfoObject> typeInfo = CreateRef<SerializableTypeInfoObject>();
            typeInfo->m_TypeName = objectInfoNode["TypeName"].as<String>();
            typeInfo->m_TypeNamespace = objectInfoNode["TypeNamespace"].as<String>();
            typeInfo->m_Flags = (ScriptFieldFlags)objectInfoNode["Flags"].as<uint32_t>();
            typeInfo->m_TypeId = objectInfoNode["TypeId"].as<uint32_t>();
            typeInfo->m_ValueType = objectInfoNode["ValueType"].as<bool>();

            Ref<SerializableObjectInfo> deserializedObjectInfo = CreateRef<SerializableObjectInfo>();
            deserializedObjectInfo->m_TypeInfo = typeInfo;
            if (result->m_ObjectInfo ==
                nullptr) // Set the object info of the SerializableObject to the first one created.
                result->m_ObjectInfo = deserializedObjectInfo;

            uint32_t fieldId = 0;
            for (const YAML::Node& field : fields)
            {
                const YAML::Node& typeInfoNode = field["TypeInfo"];
                Ref<SerializableTypeInfo> typeInfo = DeserializeTypeInfo(typeInfoNode);
                Ref<SerializableMemberInfo> memberInfo = CreateRef<SerializableFieldInfo>();

                memberInfo->m_TypeInfo = typeInfo;

                DeserializeValueYAML(field, "FieldId", memberInfo->m_FieldId, fieldId++);
                DeserializeFlagsYAML(field, "Flags", memberInfo->m_Flags, ScriptFieldFlagBits::None);
                DeserializeValueYAML(field, "ParentTypeId", memberInfo->m_ParentTypeId, 0U);
                DeserializeValueYAML(field, "Name", memberInfo->m_Name, String());

                deserializedObjectInfo->m_Fields[memberInfo->m_FieldId] = memberInfo;
                deserializedObjectInfo->m_FieldNameToId[memberInfo->m_Name] = memberInfo->m_FieldId;

                SerializableFieldKey key(memberInfo->m_ParentTypeId, memberInfo->m_FieldId);
                Ref<SerializableFieldData> data;

                switch (typeInfo->GetType())
                {
                case (SerializableType::Primitive): {
                    auto primitiveTypeInfo = std::static_pointer_cast<SerializableTypeInfoPrimitive>(typeInfo);
                    switch (primitiveTypeInfo->m_Type)
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
                    case (ScriptPrimitiveType::Vector2):
                        data = CreateRef<SerializableFieldVector2>();
                        break;
                    case (ScriptPrimitiveType::Vector3):
                        data = CreateRef<SerializableFieldVector3>();
                        break;
                    case (ScriptPrimitiveType::Vector4):
                        data = CreateRef<SerializableFieldVector4>();
                        break;
                    case (ScriptPrimitiveType::Color):
                        data = CreateRef<SerializableFieldColor>();
                        break;
                    case (ScriptPrimitiveType::Matrix4):
                        data = CreateRef<SerializableFieldMatrix4>();
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
                result->m_CachedData[key] = data;
            }

            if (lastObjectInfo != nullptr)
                lastObjectInfo->m_BaseClass = deserializedObjectInfo;
            lastObjectInfo = deserializedObjectInfo;
        }
        return result;
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
                        // This line kinda requires that the file format is valid. If we have the field info in the
                        // object info then we need to have the value. Since it's a text file, maybe consider doing some
                        // checks.
                        fieldInfo->SetValue(instance, m_CachedData[key]->GetValue(/*fieldInfo->m_TypeInfo*/));
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
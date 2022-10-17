#include "cwpch.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Serialization/ScriptSerializer.h"

#include "Crowny/Serialization/CerealDataStreamArchive.h"

CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldBool, "Bool")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldChar, "Char")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldI8, "I8")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldU8, "U8")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldI16, "I16")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldU16, "U16")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldI32, "I32")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldU32, "U32")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldI64, "I64")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldU64, "U64")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldFloat, "Float")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldDouble, "Double")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldString, "String")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldEntity, "String")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldAsset, "String")

CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableFieldData, Crowny::SerializableFieldBool)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableFieldData, Crowny::SerializableFieldChar)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableFieldData, Crowny::SerializableFieldI8)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableFieldData, Crowny::SerializableFieldU8)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableFieldData, Crowny::SerializableFieldI16)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableFieldData, Crowny::SerializableFieldU16)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableFieldData, Crowny::SerializableFieldI32)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableFieldData, Crowny::SerializableFieldU32)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableFieldData, Crowny::SerializableFieldI64)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableFieldData, Crowny::SerializableFieldU64)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableFieldData, Crowny::SerializableFieldFloat)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableFieldData, Crowny::SerializableFieldDouble)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableFieldData, Crowny::SerializableFieldString)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableFieldData, Crowny::SerializableFieldEntity)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableFieldData, Crowny::SerializableFieldAsset)

CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableTypeInfoPrimitive, "TypeInfoPrimitive")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableTypeInfoEnum, "TypeInfoEnum")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableTypeInfoObject, "TypeInfoObject")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableTypeInfoArray, "TypeInfoArray")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableTypeInfoEntity, "TypeInfoEntity")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableTypeInfoList, "TypeInfoList")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableTypeInfoDictionary, "TypeInfoDictionary")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableTypeInfoAsset, "TypeInfoAsset")

CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableTypeInfo, Crowny::SerializableTypeInfoPrimitive)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableTypeInfo, Crowny::SerializableTypeInfoEnum)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableTypeInfo, Crowny::SerializableTypeInfoObject)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableTypeInfo, Crowny::SerializableTypeInfoArray)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableTypeInfo, Crowny::SerializableTypeInfoEntity)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableTypeInfo, Crowny::SerializableTypeInfoDictionary)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableTypeInfo, Crowny::SerializableTypeInfoList)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableTypeInfo, Crowny::SerializableTypeInfoAsset)

CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldInfo, "Field")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializablePropertyInfo, "Property")
// CEREAL_REGISTER_TYPE(Crowny::SerializableFieldInfo)
// CEREAL_REGISTER_TYPE(Crowny::SerializablePropertyInfo)

CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableMemberInfo, Crowny::SerializableFieldInfo)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableMemberInfo, Crowny::SerializablePropertyInfo)

namespace Crowny
{

    struct FieldEntry
    {
        Ref<SerializableFieldKey> Key;
        Ref<SerializableFieldData> Data;
    };

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldKey& key)
    {
        archive(key.m_FieldIdx, key.m_TypeId);
    }

    template <typename Archive> void Serialize(Archive& archive, FieldEntry& entry) { archive(entry.Key, entry.Data); }

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldBool& data) { archive(data.Value); }

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldChar& data) { archive(data.Value); }

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldI8& data) { archive(data.Value); }

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldU8& data) { archive(data.Value); }

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldI16& data) { archive(data.Value); }

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldU16& data) { archive(data.Value); }

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldI32& data) { archive(data.Value); }

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldU32& data) { archive(data.Value); }

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldI64& data) { archive(data.Value); }

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldU64& data) { archive(data.Value); }

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldFloat& data) { archive(data.Value); }

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldDouble& data) { archive(data.Value); }

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldString& data)
    {
        archive(data.Value, data.Null);
    }

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldEntity& data)
    {
        archive(data.Value.GetComponent<IDComponent>().Uuid);
    }

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldAsset& data)
    {
        // this bad
    }

    /*
    template <typename Archive>
    void Serialize(Archive& archive, SerializableFieldInfo& memberInfo)
    {
        archive(cereal::base_class<SerializableMemberInfo>(&memberInfo));
        archive(memberInfo.m_Name, memberInfo.m_TypeInfo, memberInfo.m_FieldId, memberInfo.m_Flags,
    memberInfo.m_ParentTypeId);
    }

    template <typename Archive>
    void Serialize(Archive& archive, SerializablePropertyInfo& memberInfo)
    {
        archive(cereal::base_class<SerializableMemberInfo>(&memberInfo));
        archive(memberInfo.m_Name, memberInfo.m_TypeInfo, memberInfo.m_FieldId, memberInfo.m_Flags,
    memberInfo.m_ParentTypeId);
    }*/

    template <typename Archive> void Serialize(Archive& archive, SerializableObjectInfo& objectInfo)
    {
        archive(objectInfo.m_TypeInfo, objectInfo.m_BaseClass, objectInfo.m_Fields);
    }

    template <typename Archive> void Serialize(Archive& archive, SerializableTypeInfoPrimitive& primitiveInfo)
    {
        archive(primitiveInfo.m_Type);
    }

    template <typename Archive> void Serialize(Archive& archive, SerializableTypeInfoEnum& enumInfo)
    {
        archive(enumInfo.m_UnderlyingType, enumInfo.m_TypeNamespace, enumInfo.m_TypeName);
    }

    template <typename Archive> void Serialize(Archive& archive, SerializableTypeInfoArray& arrayInfo)
    {
        archive(arrayInfo.m_ElementType);
    }

    template <typename Archive> void Serialize(Archive& archive, SerializableTypeInfoObject& objectInfo)
    {
        archive(objectInfo.m_TypeName, objectInfo.m_TypeNamespace, objectInfo.m_ValueType, objectInfo.m_TypeId,
                objectInfo.m_Flags);
    }

    template <typename Archive> void Serialize(Archive& archive, SerializableTypeInfoEntity& entityInfo) {}

    template <typename Archive> void Serialize(Archive& archive, SerializableTypeInfoDictionary& entityInfo) {}

    template <typename Archive> void Serialize(Archive& archive, SerializableTypeInfoList& entityInfo) {}

    template <typename Archive> void Serialize(Archive& archive, SerializableTypeInfoAsset& entityInfo) {}

    template <typename Archive> void Save(Archive& archive, const ScriptFieldFlags& flags) { archive((uint32_t)flags); }

    template <typename Archive> void Load(Archive& archive, ScriptFieldFlags& flags)
    {
        uint32_t val = 0;
        archive(val);
        flags = (ScriptFieldFlags)val;
    }

    template <typename Archive> void Serialize(Archive& archive, SerializableMemberInfo& memberInfo)
    {
        archive(memberInfo.m_Name, memberInfo.m_TypeInfo, memberInfo.m_FieldId, memberInfo.m_Flags,
                memberInfo.m_ParentTypeId);
    }

    void ScriptSerializer::Serialize(SerializableObject* object)
    {
        /*
        YAML::Emitter out;
        out << YAML::Comment("Crowny Scene");

        out << YAML::BeginMap;
        for (auto& field : object->m_ObjectInfo)
        {
            Ref<SerializableFieldData> data = object->GetFieldData(field.second);
            out << YAML::Key << field.first->m_Field->GetName() << YAML::Value;
            field.second->Serialize(out);
        }

        out << YAML::EndSeq << YAML::EndMap;
        m_Scene->m_Filepath = filepath;*/
    }

    void Save(BinaryDataStreamOutputArchive& archive, const SerializableObject& object)
    {
        archive(object.m_ObjectInfo);
        Ref<SerializableObjectInfo> curType = object.m_ObjectInfo;

        Vector<FieldEntry> entries;
        while (curType != nullptr)
        {
            for (auto [id, field] : curType->m_Fields)
            {
                if (field->IsSerializable())
                {
                    Ref<SerializableFieldKey> key =
                      CreateRef<SerializableFieldKey>(field->m_ParentTypeId, field->m_FieldId);
                    Ref<SerializableFieldData> data = object.GetFieldData(field);
                    entries.push_back({});
                    FieldEntry& entry = entries.back();
                    entry.Key = key;
                    entry.Data = data;
                }
            }
            curType = curType->m_BaseClass;
        }
        archive(entries);
    }

    void Load(BinaryDataStreamInputArchive& archive, SerializableObject& object)
    {
        archive(object.m_ObjectInfo);
        Vector<FieldEntry> entries;
        archive(entries);
        for (const auto& fieldEntry : entries)
            object.m_CachedData[*fieldEntry.Key] = fieldEntry.Data;
    }

    // Ref<SerializableScriptObject> ScriptSerializer::Deserialize(Ref<MemoryDataStream>& stream)
    // {
    // for (auto& field : )
    // }

} // namespace Crowny
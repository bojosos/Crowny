#include "cwpch.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Scene/Scene.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scripting/Serialization/SerializableField.h"
#include "Crowny/Scripting/Serialization/SerializableObject.h"
#include "Crowny/Serialization/CerealDataStreamArchive.h"
#include "Crowny/Serialization/ScriptSerializer.h"

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
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldEntity, "Entity")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldAsset, "Asset")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldVector2, "Vector2")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldVector3, "Vector3")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldVector4, "Vector4")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldColor, "Color")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldMatrix4, "Matrix4")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldObject, "Object")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldArray, "Array")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::SerializableFieldList, "List")

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
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableFieldData, Crowny::SerializableFieldVector2)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableFieldData, Crowny::SerializableFieldVector3)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableFieldData, Crowny::SerializableFieldVector4)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableFieldData, Crowny::SerializableFieldColor)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableFieldData, Crowny::SerializableFieldMatrix4)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableFieldData, Crowny::SerializableFieldObject)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableFieldData, Crowny::SerializableFieldArray)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::SerializableFieldData, Crowny::SerializableFieldList)

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
    namespace
    {
        thread_local Scene* s_ScriptSerializationScene = nullptr;
        thread_local bool s_AssemblyQualifiedTypeMetadata = true;
    } // namespace

    ScriptTypeMetadataSerializationScope::ScriptTypeMetadataSerializationScope(bool assemblyQualified)
      : m_PreviousAssemblyQualified(s_AssemblyQualifiedTypeMetadata)
    {
        s_AssemblyQualifiedTypeMetadata = assemblyQualified;
    }

    ScriptTypeMetadataSerializationScope::~ScriptTypeMetadataSerializationScope() { s_AssemblyQualifiedTypeMetadata = m_PreviousAssemblyQualified; }

    ScriptSerializationSceneScope::ScriptSerializationSceneScope(Scene* scene) : m_PreviousScene(s_ScriptSerializationScene)
    {
        s_ScriptSerializationScene = scene;
    }

    ScriptSerializationSceneScope::~ScriptSerializationSceneScope() { s_ScriptSerializationScene = m_PreviousScene; }

    Scene* GetScriptSerializationScene()
    {
        if (s_ScriptSerializationScene != nullptr)
            return s_ScriptSerializationScene;
        if (SceneManager::TryGet() == nullptr)
            return nullptr;
        const Ref<Scene>& activeScene = SceneManager::TryGet()->GetActiveScene();
        return activeScene.get();
    }

    struct FieldEntry
    {
        Ref<SerializableFieldKey> Key;
        Ref<SerializableFieldData> Data;
    };

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldKey& key) { archive(key.m_FieldId, key.m_ParentTypeId); }

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

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldString& data) { archive(data.Value, data.Null); }

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldVector2& data) { archive(data.Value.x, data.Value.y); }

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldVector3& data)
    {
        archive(data.Value.x, data.Value.y, data.Value.z);
    }

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldVector4& data)
    {
        archive(data.Value.x, data.Value.y, data.Value.z, data.Value.w);
    }

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldColor& data)
    {
        archive(data.Value.x, data.Value.y, data.Value.z, data.Value.w);
    }

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldMatrix4& data)
    {
        for (int c = 0; c < 4; c++)
            for (int r = 0; r < 4; r++)
                archive(data.Value[c][r]);
    }

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldObject& data) { archive(data.Value, data.AllowNull); }

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldArray& data) { archive(data.ElementType, data.Values, data.Null); }

    template <typename Archive> void Serialize(Archive& archive, SerializableFieldList& data) { archive(data.ElementType, data.Values, data.Null); }

    template <typename Archive> void Save(Archive& archive, const SerializableFieldEntity& data)
    {
        UUID uuid = data.Value ? data.Value.GetComponent<IDComponent>().Uuid : UUID::EMPTY;
        archive(uuid);
    }

    template <typename Archive> void Load(Archive& archive, SerializableFieldEntity& data)
    {
        UUID uuid;
        archive(uuid);
        Scene* scene = GetScriptSerializationScene();
        if (uuid != UUID::EMPTY && scene != nullptr)
            data.Value = scene->TryGetEntityFromUuid(uuid);
    }

    template <typename Archive> void Save(Archive& archive, const SerializableFieldAsset& data)
    {
        UUID uuid = data.Value ? data.Value.GetUUID() : UUID::EMPTY;
        archive(uuid);
    }

    template <typename Archive> void Load(Archive& archive, SerializableFieldAsset& data)
    {
        UUID uuid;
        archive(uuid);
        if (uuid != UUID::EMPTY)
            data.Value = AssetManager::TryGet()->GetAssetHandle(uuid);
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

    template <typename Archive> void Serialize(Archive& archive, SerializableTypeInfoPrimitive& primitiveInfo) { archive(primitiveInfo.m_Type); }

    template <typename Archive> void Save(Archive& archive, const SerializableTypeInfoEnum& enumInfo)
    {
        archive(enumInfo.m_UnderlyingType, enumInfo.m_TypeNamespace, enumInfo.m_TypeName);
        if (s_AssemblyQualifiedTypeMetadata)
            archive(enumInfo.m_AssemblyName);
    }

    template <typename Archive> void Load(Archive& archive, SerializableTypeInfoEnum& enumInfo)
    {
        archive(enumInfo.m_UnderlyingType, enumInfo.m_TypeNamespace, enumInfo.m_TypeName);
        if (s_AssemblyQualifiedTypeMetadata)
            archive(enumInfo.m_AssemblyName);
        else
            enumInfo.m_AssemblyName.clear();
    }

    template <typename Archive> void Serialize(Archive& archive, SerializableTypeInfoArray& arrayInfo) { archive(arrayInfo.m_ElementType); }

    template <typename Archive> void Save(Archive& archive, const SerializableTypeInfoObject& objectInfo)
    {
        archive(objectInfo.m_TypeName, objectInfo.m_TypeNamespace, objectInfo.m_ValueType, objectInfo.m_TypeId, objectInfo.m_Flags);
        if (s_AssemblyQualifiedTypeMetadata)
            archive(objectInfo.m_AssemblyName);
    }

    template <typename Archive> void Load(Archive& archive, SerializableTypeInfoObject& objectInfo)
    {
        archive(objectInfo.m_TypeName, objectInfo.m_TypeNamespace, objectInfo.m_ValueType, objectInfo.m_TypeId, objectInfo.m_Flags);
        if (s_AssemblyQualifiedTypeMetadata)
            archive(objectInfo.m_AssemblyName);
        else
            objectInfo.m_AssemblyName.clear();
    }

    template <typename Archive> void Serialize(Archive& archive, SerializableTypeInfoEntity& entityInfo) {}

    template <typename Archive> void Serialize(Archive& archive, SerializableTypeInfoDictionary& dictionaryInfo)
    {
        archive(dictionaryInfo.m_KeyType, dictionaryInfo.m_ValueType);
    }

    template <typename Archive> void Serialize(Archive& archive, SerializableTypeInfoList& listInfo) { archive(listInfo.m_ElementType); }

    template <typename Archive> void Serialize(Archive& archive, SerializableTypeInfoAsset& assetInfo) { archive(assetInfo.Type); }

    template <typename Archive> void Save(Archive& archive, const ScriptFieldFlags& flags) { archive((uint32_t)flags); }

    template <typename Archive> void Load(Archive& archive, ScriptFieldFlags& flags)
    {
        uint32_t val = 0;
        archive(val);
        flags = (ScriptFieldFlags)val;
    }

    template <typename Archive> void Serialize(Archive& archive, SerializableMemberInfo& memberInfo)
    {
        archive(memberInfo.m_Name, memberInfo.m_TypeInfo, memberInfo.m_FieldId, memberInfo.m_Flags, memberInfo.m_ParentTypeId);
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
                    Ref<SerializableFieldKey> key = CreateRef<SerializableFieldKey>(field->m_ParentTypeId, field->m_FieldId);
                    Ref<SerializableFieldData> data = object.GetFieldData(field);
                    if (data == nullptr)
                    {
                        CW_ENGINE_WARN("Skipping managed field '{}' because it has no persisted value.", field->m_Name);
                        continue;
                    }
                    FieldEntry& entry = entries.emplace_back();
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
        {
            if (fieldEntry.Key == nullptr || fieldEntry.Data == nullptr)
            {
                CW_ENGINE_WARN("Skipping malformed managed field entry in persisted state.");
                continue;
            }
            object.m_CachedData[*fieldEntry.Key] = fieldEntry.Data;
        }
    }

} // namespace Crowny

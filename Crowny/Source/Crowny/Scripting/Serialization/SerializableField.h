#pragma once

#include "Crowny/Common/Yaml.h"
#include "Crowny/Scripting/Serialization/SerializableObjectInfo.h"
#include "Crowny/Scripting/Serialization/SerializableField.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntity.h"
#include "Crowny/Scripting/Mono/Mono.h"

#include "Crowny/Ecs/Entity.h"
#include "Crowny/Assets/Asset.h"


namespace Crowny
{

    class SerializableFieldKey
    {
    public:
        SerializableFieldKey() = default;
        SerializableFieldKey(uint32_t typeId, uint32_t fieldId);

        uint32_t m_TypeId;
        uint32_t m_FieldIdx;
        SerializableType m_Type;
    };

    class SerializableFieldData
    {
    public:
        SerializableFieldData() = default;
        SerializableFieldData(const Ref<SerializableTypeInfo>& typeInfo, MonoObject* value);
        SerializableFieldData(const Ref<SerializableTypeInfo>& typeInfo, MonoObject* value, bool allowNull);
        SerializableFieldData(const Ref<SerializableTypeInfo>& typeInfo);
        virtual ~SerializableFieldData() = default;

        virtual void* GetValue() = 0;
        virtual void* GetValueBoxed() = 0;
        virtual void Serialize(YAML::Emitter& out) { };
        virtual void Deserialize(const YAML::Node& node) { };

        static Ref<SerializableFieldData> Create(const Ref<SerializableTypeInfo>& typeInfo, MonoObject* value);
    };

    class SerializableFieldBool : public SerializableFieldData
    {
    public:
        void* GetValue() override { return &Value; }
        virtual void Serialize(YAML::Emitter& out) override { out << Value; }
        virtual void Deserialize(const YAML::Node& node) override { Value = node.as<bool>(); }
        bool Value = false;
    };

    class SerializableFieldChar : public SerializableFieldData
    {
    public:
        void* GetValue() override  { return &Value; }
        virtual void Serialize(YAML::Emitter& out) override { out << Value; }
        virtual void Deserialize(const YAML::Node& node) override { Value = node.as<char>(); }
        char Value = 0;
    };

    class SerializableFieldI8 : public SerializableFieldData
    {
    public:
        void* GetValue() override  { return &Value; }
        virtual void Serialize(YAML::Emitter& out) override { out << Value; }
        virtual void Deserialize(const YAML::Node& node) override { Value = node.as<int8_t>(); }
        int8_t Value = 0;
    };

    class SerializableFieldU8 : public SerializableFieldData
    {
    public:
        void* GetValue() override { return &Value; }
        virtual void Serialize(YAML::Emitter& out) override { out << Value; }
        virtual void Deserialize(const YAML::Node& node) override { Value = node.as<uint8_t>(); }
        uint8_t Value = 0U;
    };

    class SerializableFieldI16 : public SerializableFieldData
    {
    public:
        void* GetValue() override { return &Value; }
        virtual void Serialize(YAML::Emitter& out) override { out << Value; }
        virtual void Deserialize(const YAML::Node& node) override { Value = node.as<int16_t>(); }
        int16_t Value = 0;
    };

    class SerializableFieldU16 : public SerializableFieldData
    {
    public:
        void* GetValue() override { return &Value; }
        virtual void Serialize(YAML::Emitter& out) override { out << Value; }
        virtual void Deserialize(const YAML::Node& node) override { Value = node.as<uint16_t>(); }
        uint16_t Value = 0U;
    };

    class SerializableFieldI32 : public SerializableFieldData
    {
    public:
        void* GetValue() override { return &Value; }
        virtual void Serialize(YAML::Emitter& out) override { out << Value; }
        virtual void Deserialize(const YAML::Node& node) override { Value = node.as<int32_t>(); }
        int32_t Value = 0;
    };

    class SerializableFieldU32 : public SerializableFieldData
    {
    public:
        void* GetValue() override { return &Value; }
        virtual void Serialize(YAML::Emitter& out) override { out << Value; }
        virtual void Deserialize(const YAML::Node& node) override { Value = node.as<uint32_t>(); }
        uint32_t Value = 0U;
    };

    class SerializableFieldI64 : public SerializableFieldData
    {
    public:
        void* GetValue() override { return &Value; }
        virtual void Serialize(YAML::Emitter& out) override { out << Value; }
        virtual void Deserialize(const YAML::Node& node) override { Value = node.as<int64_t>(); }
        int64_t Value = 0;
    };

    class SerializableFieldU64 : public SerializableFieldData
    {
    public:
        void* GetValue() override { return &Value; }
        virtual void Serialize(YAML::Emitter& out) override { out << Value; }
        virtual void Deserialize(const YAML::Node& node) override { Value = node.as<uint64_t>(); }
        uint64_t Value = 0U;
    };

    class SerializableFieldFloat : public SerializableFieldData
    {
    public:
        void* GetValue() override { return &Value; }
        virtual void Serialize(YAML::Emitter& out) override { out << Value; }
        virtual void Deserialize(const YAML::Node& node) override { Value = node.as<float>(); }
        float Value = 0.0f;
    };

    class SerializableFieldDouble : public SerializableFieldData
    {
    public:
        void* GetValue() override { return &Value; }
        virtual void Serialize(YAML::Emitter& out) override { out << Value; }
        virtual void Deserialize(const YAML::Node& node) override { Value = node.as<double>(); }
        double Value = 0.0;
    };

    class SerializableFieldString : public SerializableFieldData
    {
    public:
        void* GetValue() override { return &Value; } // Need to do something more fancy here
        virtual void Serialize(YAML::Emitter& out) override { out << Value; }
        virtual void Deserialize(const YAML::Node& node) override { Value = node.as<String>(); }

        String Value;
        bool Null = false;
    };

    class SerializableFieldEntity : public SerializableFieldData
    {
    public:
        void* GetValue() override { return &Value; }
        // virtual void Serialize(YAML::Emitter& out) override { out << Value.GetUuid(); }
        // virtual void Deserialize(const YAML::Node& node) override { Value = SceneManager::GetActiveScene()->GetEntityFromUuid(node.as<UUID>()); }
        Entity Value;
    };

    class SerializableFieldAsset : public SerializableFieldData
    {
    public:
        void* GetValue() override { return &Value; }
        virtual void Serialize(YAML::Emitter& out) override { /* out << Value->; */ }
        Ref<Asset> Value = nullptr;
    };

    // class SerializableFieldObject : public SerializableFieldData
    // {
    //     void* GetValue() override { return &Value; }
    //     void Serialize() override { return &Value; }
    //     void Deserialize() override { return &Value; }

    //     Ref<SerializableObject> Value;
    // };

    // class SerializableFieldArray : public SerializableFieldData
    // {
    //     void* GetValue() override { return &Value; }
    //     void Serialize() override { return &Value; }
    //     void Deserialize() override { return &Value; }

    //     Ref<SerializableArray> Value;
    // };

    // class SerializableFieldList : public SerializableFieldData
    // {
    //     void* GetValue() override { return &Value; }
    //     void Serialize() override { return &Value; }
    //     void Deserialize() override { return &Value; }

    //     Ref<SerializableList> Value;
    // };

    // class SerializableFieldDictionary : public SerializableFieldData
    // {
    //     void* GetValue() override { return &Value; }
    //     uint16_t Value = false;
    //     void Serialize() override { return &Value; }
    //     void Deserialize() override { return &Value; }

    //     Ref<SerializableDictionary> Value;
    // };

}
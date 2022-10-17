#pragma once

#include "Crowny/Common/Yaml.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntity.h"
#include "Crowny/Scripting/Mono/Mono.h"
#include "Crowny/Scripting/Serialization/SerializableObjectInfo.h"

#include "Crowny/Assets/Asset.h"
#include "Crowny/Ecs/Entity.h"

namespace Crowny
{

    class SerializableObject;

    class SerializableFieldKey
    {
    public:
        SerializableFieldKey() = default;
        SerializableFieldKey(uint32_t typeId, uint32_t fieldId);

        uint32_t m_TypeId;
        uint32_t m_FieldIdx;
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
        virtual MonoObject* GetValueBoxed() = 0;
        virtual void Serialize() {}
        virtual void Deserialize() {}

        virtual void SerializeYAML(YAML::Emitter& out) const {}
        virtual void DeserializeYAML(const YAML::Node& node) {}

        static Ref<SerializableFieldData> Create(const Ref<SerializableTypeInfo>& typeInfo, MonoObject* value,
                                                 bool allowNull);
    };

    class SerializableFieldBool : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetBoolClass(), &Value); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<bool>(); }

        bool Value = false;
    };

    class SerializableFieldChar : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }
        virtual void Serialize() override {}

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<char>(); }

        char Value = 0; // Maybe consider wchar here
    };

    class SerializableFieldI8 : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<int8_t>(); }

        int8_t Value = 0;
    };

    class SerializableFieldU8 : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<uint8_t>(); }

        uint8_t Value = 0U;
    };

    class SerializableFieldI16 : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<int16_t>(); }

        int16_t Value = 0;
    };

    class SerializableFieldU16 : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<uint16_t>(); }

        uint16_t Value = 0U;
    };

    class SerializableFieldI32 : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<int32_t>(); }

        int32_t Value = 0;
    };

    class SerializableFieldU32 : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<uint32_t>(); }

        uint32_t Value = 0U;
    };

    class SerializableFieldI64 : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<int64_t>(); }

        int64_t Value = 0;
    };

    class SerializableFieldU64 : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<uint64_t>(); }

        uint64_t Value = 0U;
    };

    class SerializableFieldFloat : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<float>(); }

        float Value = 0.0f;
    };

    class SerializableFieldDouble : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<double>(); }

        double Value = 0.0;
    };

    class SerializableFieldString : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override
        {
            return MonoUtils::ToMonoString(Value);
        } // Need to do something more fancy here
        virtual MonoObject* GetValueBoxed() override { return (MonoObject*)GetValue(); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<String>(); }

        String Value;
        bool Null = false;
    };

    class SerializableFieldEntity : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return (MonoObject*)GetValue(); }

        virtual void SerializeYAML(YAML::Emitter& out) const override { out << Value.GetUuid(); }
        virtual void DeserializeYAML(const YAML::Node& node) override
        {
            SceneManager::GetActiveScene()->GetEntityFromUuid(node.as<UUID>());
        }

        Entity Value;
    };

    class SerializableFieldAsset : public SerializableFieldData
    {
    public:
        void* GetValue() override { return &Value; }
        virtual void Serialize() override
        { /* out << Value->; */
        }
        Ref<Asset> Value = nullptr;
    };

    class SerializableFieldObject : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override;
        virtual MonoObject* GetValueBoxed() override { return (MonoObject*)GetValue(); }
        virtual void Serialize() override;
        virtual void Deserialize() override;

        virtual void SerializeYAML(YAML::Emitter& out) const override;
        virtual void DeserializeYAML(const YAML::Node& node) override;

        Ref<SerializableObject> Value;
        bool AllowNull;
    };

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

} // namespace Crowny
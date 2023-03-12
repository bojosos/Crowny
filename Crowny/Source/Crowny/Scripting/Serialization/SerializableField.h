#pragma once

#include "Crowny/Common/Yaml.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntity.h"
#include "Crowny/Scripting/Mono/Mono.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"
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
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<bool>(false); }

        bool Value = false;
    };

    class SerializableFieldChar : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }
        virtual void Serialize() override {}

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<char>(0); }

        char Value = 0; // Maybe consider wchar here
    };

    class SerializableFieldI8 : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<int8_t>(0); }

        int8_t Value = 0;
    };

    class SerializableFieldU8 : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<uint8_t>(0U); }

        uint8_t Value = 0U;
    };

    class SerializableFieldI16 : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<int16_t>(0); }

        int16_t Value = 0;
    };

    class SerializableFieldU16 : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<uint16_t>(0U); }

        uint16_t Value = 0U;
    };

    class SerializableFieldI32 : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<int32_t>(0); }

        int32_t Value = 0;
    };

    class SerializableFieldU32 : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<uint32_t>(0U); }

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
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<uint64_t>(0UL); }

        uint64_t Value = 0UL;
    };

    class SerializableFieldFloat : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<float>(0.0f); }

        float Value = 0.0f;
    };

    class SerializableFieldDouble : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<double>(0.0); }

        double Value = 0.0;
    };

    class SerializableFieldVector2 : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<glm::vec2>(glm::vec2(0.0f)); }

        glm::vec2 Value = glm::vec2(0.0f);
    };

    class SerializableFieldVector3 : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<glm::vec3>(glm::vec3(0.0f)); }

        glm::vec3 Value = glm::vec3(0.0f);
    };
    class SerializableFieldVector4 : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<glm::vec4>(glm::vec4(0.0f)); }

        glm::vec4 Value = glm::vec4(0.0f);
    };
    class SerializableFieldMatrix4 : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override { return &Value; }
        virtual MonoObject* GetValueBoxed() override { return MonoUtils::Box(MonoUtils::GetCharClass(), &Value); }

        virtual void SerializeYAML(YAML::Emitter& out) const { out << Value; }
        virtual void DeserializeYAML(const YAML::Node& node) { Value = node.as<glm::mat4>(glm::mat4(1.0f)); }

        glm::mat4 Value = glm::mat4(1.0f);
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
        virtual void DeserializeYAML(const YAML::Node& node)
        {
            if (node.IsNull())
                Null = true;
            else
                Value = node.as<String>(String());
        }

        String Value;
        bool Null = false;
    };

    class SerializableFieldEntity : public SerializableFieldData
    {
    public:
        virtual void* GetValue() override
        {
            ScriptEntity* scriptEntity = ScriptSceneObjectManager::Get().GetOrCreateScriptEntity(Value);
            if (scriptEntity == nullptr)
                return nullptr;
            return scriptEntity->GetManagedInstance();
        }

        virtual MonoObject* GetValueBoxed() override { return (MonoObject*)GetValue(); }

        virtual void SerializeYAML(YAML::Emitter& out) const override
        {
            if (Value)
            {
                out << Value.GetUuid();
            }
            else
            {
                out << UUID();
            }
        }

        virtual void DeserializeYAML(const YAML::Node& node) override
        {
            // Wat? TODO: Fix this
            if (Value)
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
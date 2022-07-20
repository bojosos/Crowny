#pragma once

#include "Crowny/Scripting/Mono/MonoField.h"
#include "Crowny/Scripting/Mono/MonoProperty.h"
#include "Crowny/Scripting/Serialization/SerializableField.h"

namespace Crowny
{

    class SerializableObject
    {
    public:
        void SetFieldData(const Ref<SerializableMemberInfo>& fieldInfo, const Ref<SerializableFieldData>& val);
        Ref<SerializableFieldData> GetFieldData(const Ref<SerializableMemberInfo>& fieldInfo) const;

        static Ref<SerializableObject> CreateFromMonoObject(MonoObject* monoObject);
        static Ref<SerializableObject> CreateNew(const Ref<SerializableTypeInfoObject>& type);
        static MonoObject* CreateManagedInstance(const Ref<SerializableTypeInfoObject>& type);

        void Serialize();
        MonoObject* Deserialize();
        void Deserialize(MonoObject* instance, const Ref<SerializableObjectInfo>& objInfo);
        MonoObject* GetManagedInstance() const;

        SerializableObject() = default;
        SerializableObject(const Ref<SerializableObjectInfo>& objInfo);
        ~SerializableObject();
        SerializableObject(Ref<SerializableObjectInfo> objInfo, MonoObject* instance);

        void SerializeYAML(YAML::Emitter& out) const;
        static Ref<SerializableObject> DeserializeYAML(const YAML::Node& node);

    private:
        CW_SERIALIZABLE(SerializableObject)
        struct Hash
        {
            size_t operator()(const SerializableFieldKey& x) const;
        };
        struct Equals
        {
            bool operator()(const SerializableFieldKey& l, const SerializableFieldKey& r) const;
        };
        uint32_t m_GCHandle = 0;
        Ref<SerializableObjectInfo> m_ObjectInfo;
        UnorderedMap<SerializableFieldKey, Ref<SerializableFieldData>, Hash, Equals> m_CachedData;
    };

} // namespace Crowny
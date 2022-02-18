#pragma once

#include "Crowny/Scripting/Serialization/SerializableField.h"
#include "Crowny/Scripting/Mono/MonoField.h"
#include "Crowny/Scripting/Mono/MonoProperty.h"

namespace Crowny
{

    class SerializableMember
    {
    public:
        SerializableMember() = default;
        virtual void* GetValue(MonoObject* instance) = 0;
        virtual void SetValue(MonoObject* instance, void* data) = 0;
    };

    class SerializableField : public SerializableMember
    {
    public:
        SerializableField() = default;
        virtual void* GetValue(MonoObject* instance) override
        {
            void* value = nullptr;
            m_Field->Get(instance, value);
            return value;
        }

        virtual void SetValue(MonoObject* instance, void* data) override
        {
            m_Field->Set(instance, data);
        }
    private:
        MonoField* m_Field;
    };

    class SerializableProperty : public SerializableMember
    {
    public:
        SerializableProperty() = default;

        virtual void* GetValue(MonoObject* instance) override
        {
            return m_Property->Get(instance);
        }

        virtual void SetValue(MonoObject* instance, void* data) override
        {
            m_Property->Set(instance, data);
        }
    private:
        MonoProperty* m_Property;
    };

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

    private:
        SerializableObject(Ref<SerializableObjectInfo> objInfo, MonoObject* instance);
        SerializableObject();
        ~SerializableObject();
        
        struct Hash
        {
            size_t operator()(const SerializableFieldKey& x) const;
        };
        struct Equals
        {
            bool operator()(const SerializableFieldKey& l,const SerializableFieldKey& r) const;
        };
        uint32_t m_GCHandle = 0;
        Ref<SerializableObjectInfo> m_ObjectInfo;
        UnorderedMap<SerializableFieldKey, Ref<SerializableFieldData>, Hash, Equals> m_CachedData;
    };

}
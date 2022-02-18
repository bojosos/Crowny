#pragma once

#include "Crowny/Scripting/Mono/MonoClass.h"

namespace Crowny
{

    enum class SerializableType
    {
        Primitive,
        Enum,
        Entity,
        Asset,
        Object
    };

    enum class ScriptPrimitiveType
    {
        Bool,
        Char,
        I8,
        U8,
        I16,
        U16,
        I32,
        U32,
        I64,
        U64,
        Float,
        Double,
        String
    };

    enum class ScriptFieldFlagBits
    {
        Serializable,
        Inspectable,
        Range,
        Step,
        NotNull
    };
    typedef Flags<ScriptFieldFlagBits> ScriptFieldFlags;
    CW_FLAGS_OPERATORS(ScriptFieldFlagBits);

    class SerializableTypeInfo
    {
    public:
        virtual bool Matches(const Ref<SerializableTypeInfo>& typeInfo) const = 0;
        virtual ::MonoClass* GetMonoClass() const = 0;

        virtual SerializableType GetType() = 0;
    };

    class SerializableTypeInfoPrimitive : public SerializableTypeInfo
    {
    public:
        bool Matches(const Ref<SerializableTypeInfo>& typeInfo) const override;
        ::MonoClass* GetMonoClass() const override;
        virtual SerializableType GetType() override { return SerializableType::Primitive; }

        ScriptPrimitiveType m_Type;
    };

    class SerializableTypeInfoEnum : public SerializableTypeInfo
    {
    public:
        bool Matches(const Ref<SerializableTypeInfo>& typeInfo) const override;
        ::MonoClass* GetMonoClass() const override;
        virtual SerializableType GetType() override { return SerializableType::Enum; }

        ScriptPrimitiveType m_UnderlyingType;
        String m_TypeNamespace;
        String m_TypeName;
    };

    class SerializableTypeInfoEntity : public SerializableTypeInfo
    {
    public:
        bool Matches(const Ref<SerializableTypeInfo>& typeInfo) const override;
        ::MonoClass* GetMonoClass() const override;
        virtual SerializableType GetType() override { return SerializableType::Entity; }

        ScriptPrimitiveType m_UnderlyingType;
        String m_TypeNamespace;
        String m_TypeName;
    };

    class SerializableTypeInfoObject : public SerializableTypeInfo
    {
    public:
        bool Matches(const Ref<SerializableTypeInfo>& typeInfo) const override;
        ::MonoClass* GetMonoClass() const override;
        virtual SerializableType GetType() override { return SerializableType::Object; }

        ScriptPrimitiveType m_UnderlyingType;
        bool m_ValueType;
        uint32_t m_TypeId;
        ScriptFieldFlags m_Flags;
        String m_TypeNamespace;
        String m_TypeName;
    };

    class SerializableMemberInfo
    {
    public:
        SerializableMemberInfo() = default;
        virtual ~SerializableMemberInfo() = default;

        bool IsSerializable() const { return m_Flags.IsSet(ScriptFieldFlagBits::Serializable); }
        virtual MonoObject* GetValue(MonoObject* instance) const = 0;
        virtual void SetValue(MonoObject* instance, void* value) const = 0;
        virtual MonoObject* GetAttribute(MonoClass* monoClass) = 0;

        String m_Name;
        uint32_t m_FieldId;
        uint32_t m_ParentTypeId;
        
        Ref<SerializableTypeInfo> m_TypeInfo;
        ScriptFieldFlags m_Flags;
    };

    class SerializableFieldInfo : public SerializableMemberInfo
    {
    public:
        SerializableFieldInfo() = default;

        MonoObject* GetAttribute(MonoClass* monoClass) override;
        MonoObject* GetValue(MonoObject* instance) const override;
        void SetValue(MonoObject* instance, void* value) const override;

        MonoField* m_Field;
    };

    class SerializablePropertyInfo : public SerializableMemberInfo
    {
    public:
        SerializablePropertyInfo() = default;

        MonoObject* GetAttribute(MonoClass* monoClass) override;
        MonoObject* GetValue(MonoObject* instance) const override;
        void SetValue(MonoObject* instance, void* value) const override;
        
        MonoProperty* m_Property;
    };

    class SerializableObjectInfo
    {
    public:
        SerializableObjectInfo() = default;

        String GetFullTypeName() const { return m_TypeInfo->m_TypeNamespace + "." + m_TypeInfo->m_TypeName; }

        Ref<SerializableMemberInfo> FindMatchingField(const Ref<SerializableMemberInfo>& fieldInfo, const Ref<SerializableTypeInfo>& fieldTypeInfo) const;

        Ref<SerializableTypeInfoObject> m_TypeInfo;
        MonoClass* m_MonoClass;
        UnorderedMap<String, uint32_t> m_FieldNameToId;
        UnorderedMap<uint32_t, Ref<SerializableMemberInfo>> m_Fields;

        Ref<SerializableObjectInfo> m_BaseClass;
        Vector<std::weak_ptr<SerializableObjectInfo>> m_DerivedClasses;
    };

    class SerializableAssemblyInfo
    {
    public:
        String m_Name;

        UnorderedMap<String, uint32_t> m_TypeNameToId;
        UnorderedMap<uint32_t, Ref<SerializableObjectInfo>> m_ObjectInfos;
    };

}
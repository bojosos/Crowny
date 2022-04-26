#pragma once

#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Assets/Asset.h"

namespace Crowny
{

    enum class SerializableType
    {
        Primitive,
        Enum,
        Entity,
        Asset,
        Object,
        Array,
        List,
        Dictionary
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
        String,

        Vector2,
        Vector3,
		Vector4,
        Matrix4
    };

    enum class ScriptFieldFlagBits
    {
        Serializable = 1 << 0,
        Inspectable = 1 << 1,
        Range = 1 << 2,
        Step = 1 << 3,
        NotNull = 1 << 4
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
        Vector<String> m_EnumNames;
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

    class SerializableTypeInfoAsset : public SerializableTypeInfo
    {
    public:
		bool Matches(const Ref<SerializableTypeInfo>& typeInfo) const override { return false; }
		::MonoClass* GetMonoClass() const override { return nullptr; }
		virtual SerializableType GetType() override { return SerializableType::Asset; }
		
		AssetType Type;
    };

    class SerializableTypeInfoList : public SerializableTypeInfo
    {
    public:
        virtual bool Matches(const Ref<SerializableTypeInfo>& typeInfo) const override { return false; }
        virtual ::MonoClass* GetMonoClass() const override { return m_Class; }
        /*{
            ::MonoClass* monoClass = m_ElementType->GetMonoClass();
            if (monoClass == nullptr)
                return nullptr;
            MonoClass* genericListClass = ScriptInfoManager::Get().GetBuiltinClasses().SystemGenericListClass;
            ::MonoClass* genParams[1] = { monoClass };

            return MonoUtils::BindGenericParams(genericListClass->GetInternalPtr(), genParams, 1);
        }*/
        virtual SerializableType GetType() override { return SerializableType::List; }

        ::MonoClass* m_Class;
        Ref<SerializableTypeInfo> m_ElementType;
    };

    class SerializableTypeInfoArray : public SerializableTypeInfo
    {
    public:
		virtual bool Matches(const Ref<SerializableTypeInfo>& typeInfo) const override { return false; }
        virtual ::MonoClass* GetMonoClass() const override { return nullptr; }
        virtual SerializableType GetType() override { return SerializableType::Array; }

		Ref<SerializableTypeInfo> m_ElementType;
    };
	
    class SerializableTypeInfoDictionary : public SerializableTypeInfo
    {
    public:
		virtual bool Matches(const Ref<SerializableTypeInfo>& typeInfo) const override { return false; }
		virtual ::MonoClass* GetMonoClass() const override { return m_Class; }
		virtual SerializableType GetType() override { return SerializableType::Dictionary; }
		Ref<SerializableTypeInfo> m_KeyType;
        Ref<SerializableTypeInfo> m_ValueType;
        ::MonoClass* m_Class;
    };

    class SerializableTypeInfoObject : public SerializableTypeInfo
    {
    public:
        bool Matches(const Ref<SerializableTypeInfo>& typeInfo) const override;
        ::MonoClass* GetMonoClass() const override;
        virtual SerializableType GetType() override { return SerializableType::Object; }
        MonoObject* GetAttribute(MonoClass* monoClass);
		
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

        virtual MonoObject* GetAttribute(MonoClass* monoClass) override;
        virtual MonoObject* GetValue(MonoObject* instance) const override;
        virtual void SetValue(MonoObject* instance, void* value) const override;

        MonoField* m_Field;
    };

    class SerializablePropertyInfo : public SerializableMemberInfo
    {
    public:
        SerializablePropertyInfo() = default;

        virtual MonoObject* GetAttribute(MonoClass* monoClass) override;
        virtual MonoObject* GetValue(MonoObject* instance) const override;
        virtual void SetValue(MonoObject* instance, void* value) const override;

        MonoProperty* m_Property;
    };

    class SerializableObjectInfo
    {
    public:
        SerializableObjectInfo() = default;

        String GetFullTypeName() const { return m_TypeInfo->m_TypeNamespace + "." + m_TypeInfo->m_TypeName; }

        Ref<SerializableMemberInfo> FindMatchingField(const Ref<SerializableMemberInfo>& fieldInfo,
                                                      const Ref<SerializableTypeInfo>& fieldTypeInfo) const;

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

} // namespace Crowny
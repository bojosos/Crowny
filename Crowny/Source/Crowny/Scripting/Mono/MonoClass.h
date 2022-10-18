#pragma once

#include "Crowny/Scripting/Mono/Mono.h"

namespace Crowny
{
    class MonoClass
    {
        struct MethodId
        {
            struct Hash
            {
                size_t operator()(const MethodId& value) const;
            };

            struct Equals
            {
                bool operator()(const MethodId& a, const MethodId& b) const;
            };

            MethodId(const String& a, uint32_t paramCount);

            String Name;
            uint32_t NumParams;
        };

    public:
        MonoClass(::MonoClass* monoClass);
        ~MonoClass();
        const String& GetName() const { return m_Name; }
        const String& GetNamespace() const { return m_NamespaceName; }
        const String& GetFullName() const { return m_FullName; }

        MonoObject* CreateInstance(bool construct = true) const;
        void AddInternalCall(const String& managed, const void* func);

        const Vector<MonoMethod*>& GetMethods() const;
        const Vector<MonoField*>& GetFields() const;
        const Vector<MonoProperty*>& GetProperties() const;
        Vector<MonoClass*> GetAttributes() const;

        MonoClass* GetBaseClass() const;
        MonoObject* GetAttribute(MonoClass* monoClass) const;

        bool HasAttribute(MonoClass* monoClass) const;
        bool HasField(const String& name) const;
        bool IsSubClassOf(MonoClass* monoClass) const;
        bool IsValueType() const;

        MonoMethod* GetMethod(const String& name, uint32_t argc = 0) const;
        MonoMethod* GetMethod(const String& name, const String& signature) const;
        MonoField* GetField(const String& name) const;
        MonoProperty* GetProperty(const String& name) const;

        ::MonoClass* GetInternalPtr() const { return m_Class; }

    private:
        ::MonoClass* m_Class;
        String m_Name, m_NamespaceName, m_FullName;

        mutable bool m_AllMethodsCached, m_AllFieldsCached, m_AllPropertiesCached;

        mutable UnorderedMap<MethodId, MonoMethod*, MethodId::Hash, MethodId::Equals> m_Methods;
        mutable UnorderedMap<String, MonoField*> m_Fields;
        mutable UnorderedMap<String, MonoProperty*> m_Properties;

        mutable Vector<MonoMethod*> m_MethodList;
        mutable Vector<MonoField*> m_FieldList;
        mutable Vector<MonoProperty*> m_PropertyList;
    };

} // namespace Crowny
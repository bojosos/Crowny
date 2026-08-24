#pragma once

#include "Crowny/Common/HashedString.h"
#include "Crowny/Common/StdHeaders.h"
#include "Crowny/Common/Types.h"
#include "Crowny/Scripting/Mono/Mono.h"

namespace Crowny
{
    class MonoClass
    {
        struct MethodLookupId
        {
            MethodLookupId(HashedString name, uint32_t paramCount);
            MethodLookupId(HashedString name, HashedString signature);

            HashedString Name;
            HashedString Signature;
            uint32_t NumParams;
            bool UsesSignature;
        };

        struct MethodId
        {
            struct Hash
            {
                using is_transparent = void;

                size_t operator()(const MethodId& value) const;
                size_t operator()(const MethodLookupId& value) const;
            };

            struct Equals
            {
                using is_transparent = void;

                bool operator()(const MethodId& a, const MethodId& b) const;
                bool operator()(const MethodId& a, const MethodLookupId& b) const;
                bool operator()(const MethodLookupId& a, const MethodId& b) const;
            };

            MethodId(StringView name, uint32_t paramCount);
            MethodId(StringView name, StringView signature);

            String Name;
            String Signature;
            uint32_t NumParams;
            bool UsesSignature;
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
        bool HasField(StringView name) const;
        bool HasField(HashedString name) const;
        bool IsSubClassOf(MonoClass* monoClass) const;
        bool IsValueType() const;

        MonoMethod* GetMethod(StringView name, uint32_t argc = 0) const;
        MonoMethod* GetMethod(HashedString name, uint32_t argc = 0) const;
        MonoMethod* GetMethod(StringView name, StringView signature) const;
        MonoMethod* GetMethod(HashedString name, StringView signature) const;
        MonoField* GetField(StringView name) const;
        MonoField* GetField(HashedString name) const;
        MonoProperty* GetProperty(StringView name) const;
        MonoProperty* GetProperty(HashedString name) const;

        ::MonoClass* GetInternalPtr() const { return m_Class; }

    private:
        ::MonoClass* m_Class;
        String m_Name, m_NamespaceName, m_FullName;

        mutable bool m_AllMethodsCached, m_AllFieldsCached, m_AllPropertiesCached;

        mutable UnorderedMap<MethodId, MonoMethod*, MethodId::Hash, MethodId::Equals> m_Methods;
        mutable UnorderedMap<String, MonoField*, StringHash, StringEqual> m_Fields;
        mutable UnorderedMap<String, MonoProperty*, StringHash, StringEqual> m_Properties;

        mutable Vector<MonoMethod*> m_MethodList;
        mutable Vector<MonoField*> m_FieldList;
        mutable Vector<MonoProperty*> m_PropertyList;

        MonoMethod* GetMethodBySignature(HashedString name, HashedString signature) const;
    };

} // namespace Crowny

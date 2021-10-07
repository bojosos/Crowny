#pragma once

#include "Crowny/Scripting/CWMonoField.h"
#include "Crowny/Scripting/CWMonoMethod.h"
#include "Crowny/Scripting/CWMonoProperty.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/class.h>
#include <mono/metadata/metadata.h>
END_MONO_INCLUDE

namespace Crowny
{

    class CWMonoRuntime;

    class CWMonoClass
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
        CWMonoClass(MonoClass* monoClass);
        ~CWMonoClass();
        const String& GetName() const { return m_Name; }
        const String& GetNamespace() const { return m_NamespaceName; }
        const String& GetFullName() const { return m_FullName; }

        MonoObject* CreateInstance() const;
        void AddInternalCall(const String& managed, const void* func);

        const Vector<CWMonoMethod*>& GetMethods() const;
        const Vector<CWMonoField*>& GetFields() const;
        const Vector<CWMonoProperty*>& GetProperties() const;
        Vector<CWMonoClass*> GetAttributes() const;

        CWMonoClass* GetBaseClass() const;
        MonoObject* GetAttribute(CWMonoClass* monoClass) const;

        bool HasAttribute(CWMonoClass* monoClass) const;
        bool HasField(const String& name) const;
        bool IsSubClassOf(CWMonoClass* monoClass) const;
        bool IsValueType() const;

        CWMonoMethod* GetMethod(const String& name, uint32_t argc = 0) const;
        CWMonoMethod* GetMethod(const String& name, const String& signature) const;
        CWMonoField* GetField(const String& name) const;
        CWMonoProperty* GetProperty(const String& name) const;

        MonoClass* GetInternalPtr() const { return m_Class; }

        friend class CWMonoRuntime;

    private:
        MonoClass* m_Class;
        String m_Name, m_NamespaceName, m_FullName;

        mutable bool m_AllMethodsCached, m_AllFieldsCached, m_AllPropertiesCached;

        mutable UnorderedMap<MethodId, CWMonoMethod*, MethodId::Hash, MethodId::Equals> m_Methods;
        mutable UnorderedMap<String, CWMonoField*> m_Fields;
        mutable UnorderedMap<String, CWMonoProperty*> m_Properties;

        mutable Vector<CWMonoMethod*> m_MethodList;
        mutable Vector<CWMonoField*> m_FieldList;
        mutable Vector<CWMonoProperty*> m_PropertyList;
    };

} // namespace Crowny
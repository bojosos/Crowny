#include "cwpch.h"

#include "Crowny/Scripting/CWMonoClass.h"
#include "Crowny/Scripting/CWMonoRuntime.h"

BEGIN_MONO_INCLUDE
#include <mono/jit/jit.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/reflection.h>
END_MONO_INCLUDE

namespace Crowny
{

    CWMonoClass::MethodId::MethodId(const std::string& name, uint32_t numParams) : Name(name), NumParams(numParams) {}

    size_t CWMonoClass::MethodId::Hash::operator()(const CWMonoClass::MethodId& value) const
    {
        size_t result = 0;
        HashCombine(result, value.Name, value.NumParams);
        return result;
    }

    bool CWMonoClass::MethodId::Equals::operator()(const CWMonoClass::MethodId& a, const CWMonoClass::MethodId& b) const
    {
        return a.Name == b.Name && a.NumParams == b.NumParams;
    }

    CWMonoClass::CWMonoClass(MonoClass* monoClass)
      : m_Class(monoClass), m_AllMethodsCached(false), m_AllFieldsCached(false), m_AllPropertiesCached(false)
    {
        m_Name = mono_class_get_name(m_Class);
        m_NamespaceName = mono_class_get_namespace(monoClass);
        m_FullName = m_NamespaceName + "." + m_Name;
    }

    CWMonoClass::~CWMonoClass()
    {
        for (auto& method : m_Methods)
        {
            delete method.second;
        }
        m_Methods.clear();

        for (auto& field : m_Fields)
        {
            delete field.second;
        }
        m_Fields.clear();

        for (auto& prop : m_Properties)
        {
            delete prop.second;
        }
        m_Properties.clear();
    }
    MonoObject* CWMonoClass::CreateInstance() const
    {
        return mono_object_new(CWMonoRuntime::GetDomain(), m_Class); // call ctor
    }

    void CWMonoClass::AddInternalCall(const std::string& managed, const void* func)
    {
        mono_add_internal_call((m_FullName + "::" + managed).c_str(), func);
    }

    const std::vector<CWMonoMethod*>& CWMonoClass::GetMethods() const
    {
        if (m_AllMethodsCached)
            return m_MethodList;
        m_MethodList.clear();

        void* iter = nullptr;
        MonoMethod* method;
        while ((method = mono_class_get_methods(m_Class, &iter)))
        {
            MonoMethodSignature* sig = mono_method_signature(method);
            const char* desc = mono_signature_get_desc(sig, false);
            const char* methodName = mono_method_get_name(method);
            CWMonoMethod* curMethod = GetMethod(methodName, desc);
            m_MethodList.push_back(curMethod);
        }

        m_AllMethodsCached = true;
        return m_MethodList;
    }

    const std::vector<CWMonoField*>& CWMonoClass::GetFields() const
    {
        if (m_AllFieldsCached)
            return m_FieldList;

        m_FieldList.clear();
        void* iter = nullptr;
        MonoClassField* field;
        while ((field = mono_class_get_fields(m_Class, &iter)))
        {
            const char* name = mono_field_get_name(field);
            CWMonoField* field = GetField(name);
            m_FieldList.push_back(field);
        }

        m_AllFieldsCached = true;
        return m_FieldList;
    }

    const std::vector<CWMonoProperty*>& CWMonoClass::GetProperties() const
    {
        if (m_AllPropertiesCached)
            return m_PropertyList;

        m_PropertyList.clear();
        void* iter = nullptr;
        MonoProperty* prop;
        while ((prop = mono_class_get_properties(m_Class, &iter)))
        {
            const char* name = mono_property_get_name(prop);
            CWMonoProperty* curr = GetProperty(name);
            m_PropertyList.push_back(curr);
        }

        m_AllPropertiesCached = true;
        return m_PropertyList;
    }

    CWMonoClass* CWMonoClass::GetBaseClass() const
    {
        MonoClass* base = mono_class_get_parent(m_Class);
        if (base == nullptr)
            return nullptr;

        return new CWMonoClass(base);
    }

    std::vector<CWMonoClass*> CWMonoClass::GetAttributes() const
    {
        std::vector<CWMonoClass*> res;

        MonoCustomAttrInfo* info = mono_custom_attrs_from_class(m_Class);
        if (info == nullptr)
            return res;
        for (uint32_t i = 0; i < info->num_attrs; i++)
        {
            MonoClass* monoClass = mono_method_get_class(info->attrs[i].ctor);

            if (monoClass != nullptr)
            {
                CWMonoClass* resultClass = new CWMonoClass(monoClass);
                res.push_back(resultClass);
            }
        }

        mono_custom_attrs_free(info);

        return res;
    }

    bool CWMonoClass::HasAttribute(CWMonoClass* monoClass) const
    {
        MonoCustomAttrInfo* info = mono_custom_attrs_from_class(m_Class);
        if (info == nullptr)
            return false;
        bool hasAttr = mono_custom_attrs_has_attr(info, monoClass->GetInternalPtr()) != 0;
        mono_custom_attrs_free(info);
        return hasAttr;
    }

    bool CWMonoClass::HasField(const std::string& fieldName) const
    {
        MonoClassField* field = mono_class_get_field_from_name(m_Class, fieldName.c_str());
        return field != nullptr;
    }

    bool CWMonoClass::IsSubClassOf(CWMonoClass* monoClass) const
    {
        if (monoClass == nullptr)
            return false;

        return mono_class_is_subclass_of(m_Class, monoClass->GetInternalPtr(), true) != 0;
    }

    bool CWMonoClass::IsValueType() const { return mono_class_is_valuetype(m_Class); }

    CWMonoMethod* CWMonoClass::GetMethod(const std::string& name, const std::string& signature) const
    {
        MethodId id(name + "(" + signature + ")", 0);
        auto iterFind = m_Methods.find(id);
        if (iterFind != m_Methods.end())
            return iterFind->second;

        MonoMethod* method;
        void* iter = nullptr;
        const char* namePtr = name.c_str();
        const char* sigPtr = signature.c_str();
        while ((method = mono_class_get_methods(m_Class, &iter)))
        {
            if (strcmp(namePtr, mono_method_get_name(method)) == 0)
            {
                const char* cSig = mono_signature_get_desc(mono_method_signature(method), false);
                if (strcmp(sigPtr, cSig) == 0)
                {
                    CWMonoMethod* result = new CWMonoMethod(method);
                    m_Methods[id] = result;

                    return result;
                }
            }
        }

        return nullptr;
    }

    CWMonoMethod* CWMonoClass::GetMethod(const std::string& name, uint32_t argc) const
    {
        MethodId id(name, argc);
        auto iter = m_Methods.find(id);
        if (iter != m_Methods.end())
            return iter->second;

        MonoMethod* method = mono_class_get_method_from_name(m_Class, name.c_str(), (int)argc);
        if (method == nullptr)
            return nullptr;
        CWMonoMethod* result = new CWMonoMethod(method);
        m_Methods[id] = result;
        return result;
    }

    CWMonoField* CWMonoClass::GetField(const std::string& name) const
    {
        auto iter = m_Fields.find(name);
        if (iter != m_Fields.end())
            return iter->second;
        MonoClassField* field = mono_class_get_field_from_name(m_Class, name.c_str());
        if (field == nullptr)
            return nullptr;
        CWMonoField* result = new CWMonoField(field);
        m_Fields[name] = result;

        return result;
    }

    CWMonoProperty* CWMonoClass::GetProperty(const std::string& name) const
    {
        auto iter = m_Properties.find(name);
        if (iter != m_Properties.end())
            return iter->second;

        MonoProperty* property = mono_class_get_property_from_name(m_Class, name.c_str());
        if (property == nullptr)
            return nullptr;

        CWMonoProperty* result = new CWMonoProperty(property);
        m_Properties[name] = result;
        return result;
    }

} // namespace Crowny

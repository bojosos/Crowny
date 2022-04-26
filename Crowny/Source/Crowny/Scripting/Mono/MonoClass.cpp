#include "cwpch.h"

#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/Mono/MonoManager.h"

#include <mono/jit/jit.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/reflection.h>

namespace Crowny
{

    MonoClass::MethodId::MethodId(const String& name, uint32_t numParams) : Name(name), NumParams(numParams) {}

    size_t MonoClass::MethodId::Hash::operator()(const MonoClass::MethodId& value) const
    {
        size_t result = 0;
        HashCombine(result, value.Name, value.NumParams);
        return result;
    }

    bool MonoClass::MethodId::Equals::operator()(const MonoClass::MethodId& a, const MonoClass::MethodId& b) const
    {
        return a.Name == b.Name && a.NumParams == b.NumParams;
    }

    MonoClass::MonoClass(::MonoClass* monoClass)
      : m_Class(monoClass), m_AllMethodsCached(false), m_AllFieldsCached(false), m_AllPropertiesCached(false)
    {
        m_Name = mono_class_get_name(m_Class);
        m_NamespaceName = mono_class_get_namespace(monoClass);
        m_FullName = m_NamespaceName + "." + m_Name;
    }

    MonoClass::~MonoClass()
    {
        for (auto& method : m_Methods)
            delete method.second;
        m_Methods.clear();

        for (auto& field : m_Fields)
            delete field.second;
        m_Fields.clear();

        for (auto& prop : m_Properties)
            delete prop.second;
        m_Properties.clear();
    }
    MonoObject* MonoClass::CreateInstance(bool construct) const
    {
        MonoObject* obj = mono_object_new(MonoManager::Get().GetDomain(), m_Class);
        if (construct)
            mono_runtime_object_init(obj);
        return obj;
    }

    void MonoClass::AddInternalCall(const String& managed, const void* func)
    {
        mono_add_internal_call((m_FullName + "::" + managed).c_str(), func);
    }

    const Vector<MonoMethod*>& MonoClass::GetMethods() const
    {
        if (m_AllMethodsCached)
            return m_MethodList;
        m_MethodList.clear();

        void* iter = nullptr;
        ::MonoMethod* method;
        while ((method = mono_class_get_methods(m_Class, &iter)))
        {
            MonoMethodSignature* sig = mono_method_signature(method);
            const char* desc = mono_signature_get_desc(sig, false);
            const char* methodName = mono_method_get_name(method);
            MonoMethod* curMethod = GetMethod(methodName, desc);
            m_MethodList.push_back(curMethod);
        }

        m_AllMethodsCached = true;
        return m_MethodList;
    }

    const Vector<MonoField*>& MonoClass::GetFields() const
    {
        if (m_AllFieldsCached)
            return m_FieldList;

        m_FieldList.clear();
        void* iter = nullptr;
        MonoClassField* field;
        while ((field = mono_class_get_fields(m_Class, &iter)))
        {
            const char* name = mono_field_get_name(field);
            MonoField* field = GetField(name);
            m_FieldList.push_back(field);
        }

        m_AllFieldsCached = true;
        return m_FieldList;
    }

    const Vector<MonoProperty*>& MonoClass::GetProperties() const
    {
        if (m_AllPropertiesCached)
            return m_PropertyList;

        m_PropertyList.clear();
        void* iter = nullptr;
        ::MonoProperty* prop;
        while ((prop = mono_class_get_properties(m_Class, &iter)))
        {
            const char* name = mono_property_get_name(prop);
            MonoProperty* curr = GetProperty(name);
            m_PropertyList.push_back(curr);
        }

        m_AllPropertiesCached = true;
        return m_PropertyList;
    }

    MonoClass* MonoClass::GetBaseClass() const
    {
        ::MonoClass* base = mono_class_get_parent(m_Class);
        if (base == nullptr)
            return nullptr;

        String ns;
        String type;
        MonoUtils::GetClassName(base, ns, type);
        return MonoManager::Get().FindClass(ns, type);
    }

    Vector<MonoClass*> MonoClass::GetAttributes() const
    {
        Vector<MonoClass*> res;

        MonoCustomAttrInfo* info = mono_custom_attrs_from_class(m_Class);
        if (info == nullptr)
            return res;
        for (uint32_t i = 0; i < (uint32_t)info->num_attrs; i++)
        {
            ::MonoClass* monoClass = mono_method_get_class(info->attrs[i].ctor);

            if (monoClass != nullptr)
            {
                MonoClass* resultClass = new MonoClass(monoClass);
                res.push_back(resultClass);
            }
        }

        mono_custom_attrs_free(info);

        return res;
    }

    bool MonoClass::HasAttribute(MonoClass* monoClass) const
    {
        MonoCustomAttrInfo* info = mono_custom_attrs_from_class(m_Class);
        if (info == nullptr)
            return false;
        bool hasAttr = mono_custom_attrs_has_attr(info, monoClass->GetInternalPtr()) != 0;
        mono_custom_attrs_free(info);
        return hasAttr;
    }

    MonoObject* MonoClass::GetAttribute(MonoClass* monoClass) const
    {
        MonoCustomAttrInfo* info = mono_custom_attrs_from_class(m_Class);
        if (info == nullptr)
            return nullptr;
        MonoObject* attrs = nullptr;
        if (mono_custom_attrs_has_attr(info, monoClass->GetInternalPtr()))
            attrs = mono_custom_attrs_get_attr(info, monoClass->GetInternalPtr());
        mono_custom_attrs_free(info);
        return attrs;
    }

    bool MonoClass::HasField(const String& fieldName) const
    {
        MonoClassField* field = mono_class_get_field_from_name(m_Class, fieldName.c_str());
        return field != nullptr;
    }

    bool MonoClass::IsSubClassOf(MonoClass* monoClass) const
    {
        if (monoClass == nullptr)
            return false;

        return mono_class_is_subclass_of(m_Class, monoClass->GetInternalPtr(), true) != 0;
    }

    bool MonoClass::IsValueType() const { return mono_class_is_valuetype(m_Class); }

    MonoMethod* MonoClass::GetMethod(const String& name, const String& signature) const
    {
        MethodId id(name + "(" + signature + ")", 0);
        auto iterFind = m_Methods.find(id);
        if (iterFind != m_Methods.end())
            return iterFind->second;

        ::MonoMethod* method;
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
                    MonoMethod* result = new MonoMethod(method);
                    m_Methods[id] = result;

                    return result;
                }
            }
        }

        return nullptr;
    }

    MonoMethod* MonoClass::GetMethod(const String& name, uint32_t argc) const
    {
        MethodId id(name, argc);
        auto iter = m_Methods.find(id);
        if (iter != m_Methods.end())
            return iter->second;

        ::MonoMethod* method = mono_class_get_method_from_name(m_Class, name.c_str(), (int)argc);
        if (method == nullptr)
            return nullptr;
        MonoMethod* result = new MonoMethod(method);
        m_Methods[id] = result;
        return result;
    }

    MonoField* MonoClass::GetField(const String& name) const
    {
        auto iter = m_Fields.find(name);
        if (iter != m_Fields.end())
            return iter->second;
        MonoClassField* field = mono_class_get_field_from_name(m_Class, name.c_str());
        if (field == nullptr)
            return nullptr;
        MonoField* result = new MonoField(field);
        m_Fields[name] = result;

        return result;
    }

    MonoProperty* MonoClass::GetProperty(const String& name) const
    {
        auto iter = m_Properties.find(name);
        if (iter != m_Properties.end())
            return iter->second;

        ::MonoProperty* property = mono_class_get_property_from_name(m_Class, name.c_str());
        if (property == nullptr)
            return nullptr;

        MonoProperty* result = new MonoProperty(property);
        m_Properties[name] = result;
        return result;
    }

} // namespace Crowny

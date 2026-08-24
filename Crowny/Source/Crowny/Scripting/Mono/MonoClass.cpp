#include "cwpch.h"

#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/Mono/MonoField.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoMethod.h"
#include "Crowny/Scripting/Mono/MonoProperty.h"

#include <mono/jit/jit.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/reflection.h>

namespace Crowny
{
    namespace
    {
        void CombineHash(size_t& hash, size_t value)
        {
            hash ^= value + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }

        size_t HashMethod(HashedString name, HashedString signature, uint32_t numParams, bool usesSignature)
        {
            size_t hash = StringHash()(name);
            CombineHash(hash, std::hash<bool>()(usesSignature));
            CombineHash(hash, usesSignature ? StringHash()(signature) : std::hash<uint32_t>()(numParams));
            return hash;
        }
    } // namespace

    MonoClass::MethodLookupId::MethodLookupId(HashedString name, uint32_t paramCount)
      : Name(name), NumParams(paramCount), UsesSignature(false)
    {
    }

    MonoClass::MethodLookupId::MethodLookupId(HashedString name, HashedString signature)
      : Name(name), Signature(signature), NumParams(0), UsesSignature(true)
    {
    }

    MonoClass::MethodId::MethodId(StringView name, uint32_t numParams) : Name(name), NumParams(numParams), UsesSignature(false) {}

    MonoClass::MethodId::MethodId(StringView name, StringView signature)
      : Name(name), Signature(signature), NumParams(0), UsesSignature(true)
    {
    }

    size_t MonoClass::MethodId::Hash::operator()(const MonoClass::MethodId& value) const
    {
        return HashMethod(HashedString(value.Name), HashedString(value.Signature), value.NumParams, value.UsesSignature);
    }

    size_t MonoClass::MethodId::Hash::operator()(const MonoClass::MethodLookupId& value) const
    {
        return HashMethod(value.Name, value.Signature, value.NumParams, value.UsesSignature);
    }

    bool MonoClass::MethodId::Equals::operator()(const MonoClass::MethodId& a, const MonoClass::MethodId& b) const
    {
        return a.UsesSignature == b.UsesSignature && a.Name == b.Name &&
               (a.UsesSignature ? a.Signature == b.Signature : a.NumParams == b.NumParams);
    }

    bool MonoClass::MethodId::Equals::operator()(const MonoClass::MethodId& a, const MonoClass::MethodLookupId& b) const
    {
        if (a.UsesSignature != b.UsesSignature || !StringEqual()(StringView(a.Name), b.Name))
            return false;

        return a.UsesSignature ? StringEqual()(StringView(a.Signature), b.Signature) : a.NumParams == b.NumParams;
    }

    bool MonoClass::MethodId::Equals::operator()(const MonoClass::MethodLookupId& a, const MonoClass::MethodId& b) const
    {
        return (*this)(b, a);
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

    void MonoClass::AddInternalCall(const String& managed, const void* func) { mono_add_internal_call((m_FullName + "::" + managed).c_str(), func); }

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
        Vector<MonoClass*> result;

        MonoCustomAttrInfo* attrInfo = mono_custom_attrs_from_class(m_Class);
        if (attrInfo == nullptr)
            return result;
        result.reserve(attrInfo->num_attrs);
        for (uint32_t i = 0; i < (uint32_t)attrInfo->num_attrs; i++)
        {
            ::MonoClass* attributeClass = mono_method_get_class(attrInfo->attrs[i].ctor);
            MonoClass* monoClass = MonoManager::Get().FindClass(attributeClass);

            if (monoClass != nullptr)
                result.push_back(monoClass);
        }

        mono_custom_attrs_free(attrInfo);

        return result;
    }

    bool MonoClass::HasAttribute(MonoClass* monoClass) const
    {
        MonoCustomAttrInfo* info = mono_custom_attrs_from_class(m_Class);
        if (info == nullptr)
            return false;
        const bool hasAttr = mono_custom_attrs_has_attr(info, monoClass->GetInternalPtr()) != 0;
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

    bool MonoClass::HasField(StringView fieldName) const { return HasField(HashedString(fieldName)); }

    bool MonoClass::HasField(HashedString fieldName) const { return GetField(fieldName) != nullptr; }

    bool MonoClass::IsSubClassOf(MonoClass* monoClass) const
    {
        if (monoClass == nullptr)
            return false;

        return mono_class_is_subclass_of(m_Class, monoClass->GetInternalPtr(), true) != 0;
    }

    bool MonoClass::IsValueType() const { return mono_class_is_valuetype(m_Class); }

    MonoMethod* MonoClass::GetMethod(StringView name, StringView signature) const
    {
        return GetMethodBySignature(HashedString(name), HashedString(signature));
    }

    MonoMethod* MonoClass::GetMethod(HashedString name, StringView signature) const
    {
        return GetMethodBySignature(name, HashedString(signature));
    }

    MonoMethod* MonoClass::GetMethodBySignature(HashedString name, HashedString signature) const
    {
        const MethodLookupId lookup(name, signature);
        const auto iterFind = m_Methods.find(lookup);
        if (iterFind != m_Methods.end())
            return iterFind->second;

        MethodId id(name.GetView(), signature.GetView());
        ::MonoMethod* method;
        void* iter = nullptr;
        const char* namePtr = id.Name.c_str();
        const char* sigPtr = id.Signature.c_str();
        while ((method = mono_class_get_methods(m_Class, &iter)))
        {
            if (strcmp(namePtr, mono_method_get_name(method)) == 0)
            {
                const char* cSig = mono_signature_get_desc(mono_method_signature(method), false);
                if (strcmp(sigPtr, cSig) == 0)
                {
                    MonoMethod* result = new MonoMethod(method);
                    m_Methods.emplace(std::move(id), result);

                    return result;
                }
            }
        }

        m_Methods.emplace(std::move(id), nullptr);
        return nullptr;
    }

    MonoMethod* MonoClass::GetMethod(StringView name, uint32_t argc) const { return GetMethod(HashedString(name), argc); }

    MonoMethod* MonoClass::GetMethod(HashedString name, uint32_t argc) const
    {
        const MethodLookupId lookup(name, argc);
        const auto iter = m_Methods.find(lookup);
        if (iter != m_Methods.end())
            return iter->second;

        MethodId id(name.GetView(), argc);
        ::MonoMethod* method = mono_class_get_method_from_name(m_Class, id.Name.c_str(), (int)argc);
        if (method == nullptr)
        {
            m_Methods.emplace(std::move(id), nullptr);
            return nullptr;
        }
        MonoMethod* result = new MonoMethod(method);
        m_Methods.emplace(std::move(id), result);
        return result;
    }

    MonoField* MonoClass::GetField(StringView name) const { return GetField(HashedString(name)); }

    MonoField* MonoClass::GetField(HashedString name) const
    {
        const auto iter = m_Fields.find(name);
        if (iter != m_Fields.end())
            return iter->second;

        String ownedName(name.GetView());
        MonoClassField* field = mono_class_get_field_from_name(m_Class, ownedName.c_str());
        if (field == nullptr)
        {
            m_Fields.emplace(std::move(ownedName), nullptr);
            return nullptr;
        }
        MonoField* result = new MonoField(field);
        m_Fields.emplace(std::move(ownedName), result);

        return result;
    }

    MonoProperty* MonoClass::GetProperty(StringView name) const { return GetProperty(HashedString(name)); }

    MonoProperty* MonoClass::GetProperty(HashedString name) const
    {
        const auto iter = m_Properties.find(name);
        if (iter != m_Properties.end())
            return iter->second;

        String ownedName(name.GetView());
        ::MonoProperty* property = mono_class_get_property_from_name(m_Class, ownedName.c_str());
        if (property == nullptr)
        {
            m_Properties.emplace(std::move(ownedName), nullptr);
            return nullptr;
        }

        MonoProperty* result = new MonoProperty(property);
        m_Properties.emplace(std::move(ownedName), result);
        return result;
    }

} // namespace Crowny

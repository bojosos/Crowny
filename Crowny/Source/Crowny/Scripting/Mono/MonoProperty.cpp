#include "cwpch.h"

#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoMethod.h"
#include "Crowny/Scripting/Mono/MonoProperty.h"

#include <mono/metadata/object.h>
#include <mono/metadata/reflection.h>

namespace Crowny
{

    MonoProperty::MonoProperty(::MonoProperty* prop)
      : m_Property(prop), m_IsIndexed(false), m_IsFullyInitialized(false), m_ReturnType(nullptr)
    {
        m_GetMethod = mono_property_get_get_method(m_Property);
        m_SetMethod = mono_property_get_set_method(m_Property);

        m_Name = mono_property_get_name(m_Property);
    }

    MonoObject* MonoProperty::Get(MonoObject* instance) const
    {
        if (m_GetMethod == nullptr)
            return nullptr;
        return mono_runtime_invoke(m_GetMethod, instance, nullptr, nullptr);
    }

    MonoObject* MonoProperty::GetIndexed(MonoObject* instance, uint32_t index) const
    {
        void* args[1];
        args[0] = &index;
        return mono_runtime_invoke(m_GetMethod, instance, args, nullptr);
    }

    void MonoProperty::SetIndexed(MonoObject* instance, uint32_t index, void* value) const
    {
        void* args[2];
        args[0] = &index;
        args[1] = value; // Maybe could get away with using mono_property_set_value and then won't need to box/unbox
        mono_runtime_invoke(m_SetMethod, instance, args, nullptr);
    }

    void MonoProperty::Set(MonoObject* instance, void* value) const
    {
        if (m_SetMethod == nullptr)
            return;

        void* args[1];
        args[0] = value; // Maybe could get away with using mono_property_set_value and then won't need to box/unbox
        mono_runtime_invoke(m_SetMethod, instance, args, nullptr);
    }

    void MonoProperty::InitializeDeferred() const
    {
        if (m_GetMethod != nullptr)
        {
            MonoMethodSignature* signature = mono_method_signature(m_GetMethod);
            MonoType* returnType = mono_signature_get_return_type(signature);
            if (returnType != nullptr)
            {
                ::MonoClass* returnClass = mono_class_from_mono_type(returnType);
                if (returnClass != nullptr)
                    m_ReturnType = MonoManager::Get().FindClass(returnClass);
            }
            uint32_t numParams = mono_signature_get_param_count(signature);
            m_IsIndexed = numParams == 1;
        }
        else if (m_SetMethod != nullptr)
        {
            MonoMethodSignature* signature = mono_method_signature(m_SetMethod);
            MonoType* returnType = mono_signature_get_return_type(signature);
            if (returnType != nullptr)
            {
                ::MonoClass* returnClass = mono_class_from_mono_type(returnType);
                if (returnClass != nullptr)
                    m_ReturnType = MonoManager::Get().FindClass(returnClass);
            }

            uint32_t numParams = mono_signature_get_param_count(signature);
            m_IsIndexed = numParams == 2;
        }

        m_IsFullyInitialized = true;
    }

    bool MonoProperty::IsIndexed() const
    {
        if (!m_IsFullyInitialized)
            InitializeDeferred();

        return m_IsIndexed;
    }

    MonoClass* MonoProperty::GetReturnType() const
    {
        if (!m_IsFullyInitialized)
            InitializeDeferred();

        return m_ReturnType;
    }

    bool MonoProperty::HasAttribute(MonoClass* monoClass) const
    {
        ::MonoClass* parent = mono_property_get_parent(m_Property);
        MonoCustomAttrInfo* attrInfo = mono_custom_attrs_from_property(parent, m_Property);
        if (attrInfo == nullptr || monoClass == nullptr)
            return false;
        bool hasAttr = mono_custom_attrs_has_attr(attrInfo, monoClass->GetInternalPtr()) != 0;
        mono_custom_attrs_free(attrInfo);

        return hasAttr;
    }

    MonoObject* MonoProperty::GetAttribute(MonoClass* monoClass) const
    {
        ::MonoClass* parent = mono_property_get_parent(m_Property);
        MonoCustomAttrInfo* attrInfo = mono_custom_attrs_from_property(parent, m_Property);
        if (attrInfo == nullptr || monoClass == nullptr)
            return nullptr;
        MonoObject* attr = nullptr;
        if (mono_custom_attrs_has_attr(attrInfo, monoClass->GetInternalPtr()))
            attr = mono_custom_attrs_get_attr(attrInfo, monoClass->GetInternalPtr());

        mono_custom_attrs_free(attrInfo);
        return attr;
    }

    Vector<MonoClass*> MonoProperty::GetAttributes() const
    {
        Vector<MonoClass*> result;

        ::MonoClass* parent = mono_property_get_parent(m_Property);
        MonoCustomAttrInfo* attrInfo = mono_custom_attrs_from_property(parent, m_Property);
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

    CrownyMonoVisibility MonoProperty::GetVisibility() const
    {
        CrownyMonoVisibility getterVisibility = CrownyMonoVisibility::Public;
        if (m_GetMethod)
        {
            MonoMethod tmp(m_GetMethod);
            getterVisibility = tmp.GetVisibility();
        }

        CrownyMonoVisibility setterVisibility = CrownyMonoVisibility::Public;
        if (m_SetMethod)
        {
            MonoMethod tmp(m_SetMethod);
            setterVisibility = tmp.GetVisibility();
        }
        if (getterVisibility < setterVisibility)
            return getterVisibility;
        return setterVisibility;
    }

} // namespace Crowny
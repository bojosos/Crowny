#include "cwpch.h"

#include "Crowny/Scripting/CWMonoClass.h"
#include "Crowny/Scripting/CWMonoProperty.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/object.h>
#include <mono/metadata/reflection.h>
END_MONO_INCLUDE

namespace Crowny
{

    CWMonoProperty::CWMonoProperty(MonoProperty* prop)
      : m_Property(prop), m_IsIndexed(false), m_IsFullyInitialized(false), m_ReturnType(nullptr)
    {
        m_GetMethod = mono_property_get_get_method(m_Property);
        m_SetMethod = mono_property_get_set_method(m_Property);

        m_Name = mono_property_get_name(m_Property);
    }

    MonoObject* CWMonoProperty::Get(MonoObject* instance) const
    {
        if (m_GetMethod == nullptr)
            return nullptr;
        return mono_runtime_invoke(m_GetMethod, instance, nullptr, nullptr);
    }

    MonoObject* CWMonoProperty::GetIndexed(MonoObject* instance, uint32_t index) const
    {
        void* args[1];
        args[0] = &index;
        return mono_runtime_invoke(m_GetMethod, instance, args, nullptr);
    }

    void CWMonoProperty::SetIndexed(MonoObject* instance, uint32_t index, void* value) const
    {
        void* args[2];
        args[0] = &index;
        args[1] = value;
        mono_runtime_invoke(m_SetMethod, instance, args, nullptr);
    }

    void CWMonoProperty::Set(MonoObject* instance, void* value) const
    {
        if (m_SetMethod == nullptr)
            return;

        void* args[1];
        args[0] = value;
        mono_runtime_invoke(m_SetMethod, instance, args, nullptr);
    }

    void CWMonoProperty::InitializeDeferred() const
    {
        if (m_GetMethod != nullptr)
        {
            MonoMethodSignature* sig = mono_method_signature(m_GetMethod);
            MonoType* retType = mono_signature_get_return_type(sig);
            if (retType != nullptr)
            {
                MonoClass* retClass = mono_class_from_mono_type(retType);
                if (retClass != nullptr)
                    m_ReturnType = new CWMonoClass(retClass);
            }
            uint32_t numParams = mono_signature_get_param_count(sig);
            m_IsIndexed = numParams == 1;
        }
        else if (m_SetMethod != nullptr)
        {
            MonoMethodSignature* sig = mono_method_signature(m_SetMethod);
            MonoType* retType = mono_signature_get_return_type(sig);
            if (retType != nullptr)
            {
                MonoClass* retClass = mono_class_from_mono_type(retType);
                if (retClass != nullptr)
                    m_ReturnType = new CWMonoClass(retClass);
            }

            uint32_t numParams = mono_signature_get_param_count(sig);
            m_IsIndexed = numParams == 2;
        }

        m_IsFullyInitialized = true;
    }

    bool CWMonoProperty::IsIndexed() const
    {
        if (!m_IsFullyInitialized)
            InitializeDeferred();

        return m_IsIndexed;
    }

    CWMonoClass* CWMonoProperty::GetReturnType() const
    {
        if (!m_IsFullyInitialized)
            InitializeDeferred();

        return m_ReturnType;
    }

    MonoObject* CWMonoProperty::GetAttribute(CWMonoClass* monoClass) const
    {
        MonoClass* parent = mono_property_get_parent(m_Property);
        MonoCustomAttrInfo* info = mono_custom_attrs_from_property(parent, m_Property);
        if (info == nullptr)
            return nullptr;
        MonoObject* attr = nullptr;
        if (mono_custom_attrs_has_attr(info, monoClass->GetInternalPtr()))
            attr = mono_custom_attrs_get_attr(info, monoClass->GetInternalPtr());

        mono_custom_attrs_free(info);
        return attr;
    }

    bool CWMonoProperty::HasAttribute(CWMonoClass* monoClass) const
    {
        MonoClass* parent = mono_property_get_parent(m_Property);
        MonoCustomAttrInfo* info = mono_custom_attrs_from_property(parent, m_Property);
        if (info == nullptr)
            return false;
        bool hasAttr = mono_custom_attrs_has_attr(info, monoClass->GetInternalPtr()) != 0;
        mono_custom_attrs_free(info);

        return hasAttr;
    }

    CWMonoVisibility CWMonoProperty::GetVisibility() const
    {
        CWMonoVisibility getterVisibility = CWMonoVisibility::Public;
        if (m_GetMethod)
        {
            CWMonoMethod tmp(m_GetMethod);
            getterVisibility = tmp.GetVisibility();
        }

        CWMonoVisibility setterVisibility = CWMonoVisibility::Public;
        if (m_SetMethod)
        {
            CWMonoMethod tmp(m_SetMethod);
            setterVisibility = tmp.GetVisibility();
        }
        if (getterVisibility < setterVisibility)
            return getterVisibility;
        return setterVisibility;
    }

} // namespace Crowny
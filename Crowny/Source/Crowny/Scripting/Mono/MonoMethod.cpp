#include "cwpch.h"

#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoMethod.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"

#include <mono/metadata/attrdefs.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/loader.h>
#include <mono/metadata/object.h>
#include <mono/metadata/reflection.h>

namespace Crowny
{

    MonoMethod::MonoMethod(::MonoMethod* method)
      : m_Method(method), m_CachedParams(nullptr), m_CachedReturnType(nullptr), m_HasCachedSignature(false),
        m_CachedNumParams(0)
    {
        m_Name = mono_method_get_name(m_Method);
    }

    MonoClass* MonoMethod::GetParameterType(uint32_t idx) const
    {
        if (!m_HasCachedSignature)
            CacheSignature();
        if (idx >= m_CachedNumParams)
        {
            CW_ENGINE_ERROR("Param index out of range.");
            return nullptr;
        }
        return m_CachedParams[idx];
    }

    MonoObject* MonoMethod::Invoke(MonoObject* instance, void** params)
    {
        MonoObject* exception = nullptr;
        MonoObject* ret = mono_runtime_invoke(m_Method, instance, params, &exception);
        MonoUtils::CheckException(exception);
        return ret;
    }

    void* MonoMethod::GetThunk() const { return mono_method_get_unmanaged_thunk(m_Method); }

    bool MonoMethod::HasAttribute(MonoClass* monoClass) const
    {
        MonoCustomAttrInfo* info = mono_custom_attrs_from_method(m_Method);
        if (info == nullptr)
            return false;
        bool hasAttrs = mono_custom_attrs_has_attr(info, monoClass->GetInternalPtr()) != 0;
        mono_custom_attrs_free(info);
        return hasAttrs;
    }

    MonoObject* MonoMethod::GetAttribute(MonoClass* monoClass) const
    {
        MonoCustomAttrInfo* info = mono_custom_attrs_from_method(m_Method);
        if (info == nullptr)
            return nullptr;

        MonoObject* attrs = nullptr;
        if (mono_custom_attrs_has_attr(info, monoClass->GetInternalPtr()))
        {
            attrs = mono_custom_attrs_get_attr(info, monoClass->GetInternalPtr());
        }
        mono_custom_attrs_free(info);
        return attrs;
    }

    Vector<MonoClass*> MonoMethod::GetAttributes() const
    {
        Vector<MonoClass*> result;

        MonoCustomAttrInfo* attrInfo = mono_custom_attrs_from_method(m_Method);
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

    bool MonoMethod::IsStatic() const
    {
        if (!m_HasCachedSignature)
            CacheSignature();
        return m_IsStatic;
    }

    const String& MonoMethod::GetFullDeclName() const
    {
        if (!m_HasCachedSignature)
            CacheSignature();
        return m_FullDeclName;
    }

    MonoClass* MonoMethod::GetReturnType() const
    {
        if (!m_HasCachedSignature)
            CacheSignature();
        return m_CachedReturnType;
    }

    void MonoMethod::CacheSignature() const
    {
        MonoMethodSignature* signature = mono_method_signature(m_Method);
        MonoType* returnType = mono_signature_get_return_type(signature);
        if (returnType != nullptr)
        {
            ::MonoClass* returnTypeClass = mono_class_from_mono_type(returnType);
            if (returnTypeClass != nullptr)
                m_CachedReturnType = MonoManager::Get().FindClass(returnTypeClass);
        }

        m_CachedNumParams = (uint32_t)mono_signature_get_param_count(signature);
        if (m_CachedParams != nullptr)
        {
            delete[] m_CachedParams;
            m_CachedParams = nullptr;
        }

        if (m_CachedNumParams > 0)
        {
            m_CachedParams = new MonoClass*[m_CachedNumParams];
            void* iter = nullptr;
            for (uint32_t i = 0; i < m_CachedNumParams; i++)
            {
                MonoType* curParamType = mono_signature_get_params(signature, &iter);
                ::MonoClass* returnTypeClass = mono_class_from_mono_type(curParamType);
                m_CachedParams[i] = MonoManager::Get().FindClass(returnTypeClass);
            }
        }

        m_IsStatic = !mono_signature_is_instance(signature);
        m_FullDeclName = CrownyMonoVisibilityToString(GetVisibility()) + (m_IsStatic ? " static " : " ") +
                         mono_method_full_name(m_Method, true);

        m_HasCachedSignature = true;
    }

    uint32_t MonoMethod::GetNumParams() const
    {
        if (!m_HasCachedSignature)
            CacheSignature();
        return m_CachedNumParams;
    }

    CrownyMonoVisibility MonoMethod::GetVisibility() const
    {
        uint32_t flags = mono_method_get_flags(m_Method, nullptr) & MONO_METHOD_ATTR_ACCESS_MASK;

        switch (flags)
        {
        case (MONO_METHOD_ATTR_PRIVATE):
            return CrownyMonoVisibility::Private;
        case (MONO_METHOD_ATTR_FAM_AND_ASSEM):
            return CrownyMonoVisibility::ProtectedInternal;
        case (MONO_METHOD_ATTR_ASSEM):
            return CrownyMonoVisibility::Internal;
        case (MONO_METHOD_ATTR_FAMILY):
            return CrownyMonoVisibility::Protected;
        case (MONO_METHOD_ATTR_PUBLIC):
            return CrownyMonoVisibility::Public;
        case (MONO_METHOD_ATTR_COMPILER_CONTROLLED):
            return CrownyMonoVisibility::Internal;
        }

        CW_ENGINE_ASSERT(false, "Unknown visibility.");
        return CrownyMonoVisibility::Private;
    }

} // namespace Crowny
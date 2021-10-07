#include "cwpch.h"

#include "Crowny/Scripting/CWMonoClass.h"
#include "Crowny/Scripting/CWMonoMethod.h"
#include "Crowny/Scripting/MonoUtils.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/attrdefs.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/loader.h>
#include <mono/metadata/object.h>
#include <mono/metadata/reflection.h>
END_MONO_INCLUDE

namespace Crowny
{

    CWMonoMethod::CWMonoMethod(MonoMethod* method) : m_Method(method)
    {
        m_Signature = mono_method_signature(m_Method);
        m_Name = mono_method_get_name(m_Method);

        m_FullDeclName = CWMonoVisibilityToString(GetVisibility()) + (IsStatic() ? " static " : " ") +
                         mono_method_full_name(m_Method, true);
    }

    Vector<CWMonoClass*> CWMonoMethod::GetParameterTypes()
    {
        void* iter = nullptr;
        MonoType* type;
        Vector<CWMonoClass*> res;
        while ((type = mono_signature_get_params(m_Signature, &iter)))
        {
            res.emplace_back(new CWMonoClass(mono_type_get_class(type)));
        }

        return res;
    }

    MonoObject* CWMonoMethod::Invoke(MonoObject* instance, void** params)
    {
        MonoObject* exception = nullptr;
        MonoObject* ret = mono_runtime_invoke(m_Method, instance, params, &exception);
        MonoUtils::CheckException(exception);
        return ret;
    }

    void* CWMonoMethod::GetThunk() const { return mono_method_get_unmanaged_thunk(m_Method); }

    bool CWMonoMethod::HasAttribute(CWMonoClass* monoClass) const
    {
        MonoCustomAttrInfo* info = mono_custom_attrs_from_method(m_Method);
        if (info == nullptr)
            return false;
        bool hasAttrs = mono_custom_attrs_has_attr(info, monoClass->GetInternalPtr()) != 0;
        mono_custom_attrs_free(info);
        return hasAttrs;
    }

    MonoObject* CWMonoMethod::GetAttribute(CWMonoClass* monoClass) const
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

    bool CWMonoMethod::IsStatic() { return (mono_method_get_flags(m_Method, nullptr) & MONO_METHOD_ATTR_STATIC) != 0; }

    bool CWMonoMethod::IsVirtual()
    {
        return (mono_method_get_flags(m_Method, nullptr) & MONO_METHOD_ATTR_VIRTUAL) != 0;
    }

    CWMonoClass* CWMonoMethod::GetReturnType()
    {
        return new CWMonoClass(mono_type_get_class(mono_signature_get_return_type(m_Signature)));
    }

    CWMonoVisibility CWMonoMethod::GetVisibility()
    {
        uint32_t flags = mono_method_get_flags(m_Method, nullptr) & MONO_METHOD_ATTR_ACCESS_MASK;

        switch (flags)
        {
        case (MONO_METHOD_ATTR_PRIVATE):
            return CWMonoVisibility::Private;
        case (MONO_METHOD_ATTR_FAM_AND_ASSEM):
            return CWMonoVisibility::ProtectedInternal;
        case (MONO_METHOD_ATTR_ASSEM):
            return CWMonoVisibility::Internal;
        case (MONO_METHOD_ATTR_FAMILY):
            return CWMonoVisibility::Protected;
        case (MONO_METHOD_ATTR_PUBLIC):
            return CWMonoVisibility::Public;
        }

        CW_ENGINE_ASSERT(false, "Unknown visibility.");
        return CWMonoVisibility::Private;
    }

} // namespace Crowny
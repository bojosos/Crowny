#include "cwpch.h"

#include "Crowny/Scripting/CWMonoMethod.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/loader.h>
#include <mono/metadata/attrdefs.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/object.h>
END_MONO_INCLUDE

namespace Crowny
{

	CWMonoMethod::CWMonoMethod(MonoMethod* method) : m_Method(method)
	{
		m_Signature = mono_method_signature(m_Method);
		m_Name = mono_method_get_name(m_Method);
		
		m_FullDeclName = CWMonoVisibilityToString(GetVisibility()) + (IsStatic() ? " static " : " ")  + mono_method_full_name(m_Method, true);
	}

	std::vector<CWMonoType> CWMonoMethod::GetParameterTypes()
	{
		void* iter = nullptr;
		MonoType* type;
		std::vector<CWMonoType> res;
		while ((type = mono_signature_get_params(m_Signature, &iter)))
		{
			res.emplace_back(type);
		}

		return res;
	}

	void CWMonoMethod::Call(MonoObject* instance)
	{
		MonoObject* exception = nullptr;
		mono_runtime_invoke(m_Method, instance, nullptr, &exception);
		if (exception != nullptr)
		{
			MonoClass* exceptionClass = mono_object_get_class(exception);
			MonoProperty* exceptionProp = mono_class_get_property_from_name(exceptionClass, "Message");
			MonoMethod* exceptionMsgGetter = mono_property_get_get_method(exceptionProp);
			MonoString* exceptionMsg = (MonoString*)mono_runtime_invoke(exceptionMsgGetter, exception, nullptr, nullptr);
			
			MonoProperty* exceptionStackProp = mono_class_get_property_from_name(exceptionClass, "StackTrace");
			MonoMethod* exceptionStackGetter = mono_property_get_get_method(exceptionStackProp);
			MonoString* exceptionStackTrace = (MonoString*)mono_runtime_invoke(exceptionStackGetter, exception, nullptr, nullptr);
			
			CW_ENGINE_CRITICAL("Managed exception: {0}", mono_string_to_utf8(exceptionStackTrace));
		}
	}

	bool CWMonoMethod::IsStatic()
	{
		return (mono_method_get_flags(m_Method, nullptr) & MONO_METHOD_ATTR_STATIC) != 0;
	}

	bool CWMonoMethod::IsVirtual()
	{
		return (mono_method_get_flags(m_Method, nullptr) & MONO_METHOD_ATTR_VIRTUAL) != 0;
	}

	CWMonoType CWMonoMethod::GetReturnType()
	{
		return CWMonoType(mono_signature_get_return_type(m_Signature));
	}

	CWMonoVisibility CWMonoMethod::GetVisibility()
	{
		uint32_t flags = mono_method_get_flags(m_Method, nullptr) & MONO_METHOD_ATTR_ACCESS_MASK;

		switch (flags)
		{
		case(MONO_METHOD_ATTR_PRIVATE):			return CWMonoVisibility::PRIVATE;
		case(MONO_METHOD_ATTR_FAM_AND_ASSEM):   return CWMonoVisibility::PROTECTED_INTERNAL;
		case(MONO_METHOD_ATTR_ASSEM):			return CWMonoVisibility::INTERNAL;
		case(MONO_METHOD_ATTR_FAMILY):			return CWMonoVisibility::PROTECTED;
		case(MONO_METHOD_ATTR_PUBLIC):			return CWMonoVisibility::PUBLIC;
		}

		CW_ENGINE_ERROR("Unkown visibility");
		return CWMonoVisibility::PRIVATE;
	}

}
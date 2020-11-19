#include "cwpch.h"

#include "CWMonoClass.h"
#include "CWMonoRuntime.h"

BEGIN_MONO_INCLUDE
#include <mono/jit/jit.h>
#include <mono/metadata/debug-helpers.h>
END_MONO_INCLUDE

namespace Crowny
{

	CWMonoClass::CWMonoClass(MonoClass* monoClass) : m_Class(monoClass)
	{
		m_Name = mono_class_get_name(m_Class);
		m_NamespaceName = mono_class_get_namespace(monoClass);
	}

	MonoObject* CWMonoClass::CreateInstance()
	{
		return mono_object_new(CWMonoRuntime::GetDomain(), m_Class);
	}

	void CWMonoClass::AddInternalCall(const std::string& managed, const void* func)
	{
		mono_add_internal_call((m_NamespaceName + "." + m_Name + "::" + managed).c_str(), func);
	}

	std::vector<CWMonoMethod*> CWMonoClass::GetMethods()
	{
		void* iter = nullptr;
		MonoMethod* method;
		std::vector<CWMonoMethod*> res(mono_class_num_fields(m_Class));

		while (method = mono_class_get_methods(m_Class, &iter))
		{
			res.emplace_back(new CWMonoMethod(method));
		}

		return res;
	}

	std::vector<CWMonoField*> CWMonoClass::GetFields()
	{
		void* iter = nullptr;
		MonoClassField* field;
		std::vector<CWMonoField*> res;

		while (field = mono_class_get_fields(m_Class, &iter))
		{
			res.emplace_back(new CWMonoField(field));
		}

		return res;
	}

	CWMonoMethod* CWMonoClass::GetMethod(const std::string& nameWithArgs)
	{
		MonoMethodDesc* desc = mono_method_desc_new((":" + nameWithArgs).c_str(), 0);
		auto* res = new CWMonoMethod(mono_method_desc_search_in_class(desc, m_Class));
		mono_method_desc_free(desc);

		return res;
	}

	CWMonoMethod* CWMonoClass::GetMethod(const std::string& name, uint32_t argc)
	{
		return new CWMonoMethod(mono_class_get_method_from_name(m_Class, name.c_str(), argc));
	}

	CWMonoField* CWMonoClass::GetField(const std::string& name)
	{
		return new CWMonoField(mono_class_get_field_from_name(m_Class, name.c_str()));
	}

}
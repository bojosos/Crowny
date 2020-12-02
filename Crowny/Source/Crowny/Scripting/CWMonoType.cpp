#include "cwpch.h"

#include "Crowny/Scripting/CWMonoType.h"
#include "Crowny/Scripting/CWMonoRuntime.h"

namespace Crowny
{

	CWMonoType::CWMonoType(MonoType* type) : m_Type(type)
	{
		m_Name = mono_type_get_name(m_Type);
	}

	bool CWMonoType::IsValueType()
	{
		return !!mono_class_is_valuetype(mono_type_get_class(m_Type));
	}

}
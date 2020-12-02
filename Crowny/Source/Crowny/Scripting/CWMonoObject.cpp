#include "cwpch.h"

#include "Crowny/Scripting/CWMonoObject.h"

namespace Crowny
{
	
	CWMonoObject::CWMonoObject(MonoObject* monoObj) : m_Object(monoObj)
	{
		mono_runtime_object_init(m_Object);
	}

	bool CWMonoObject::Valid() const
	{
		return m_Object != nullptr;
	}

}
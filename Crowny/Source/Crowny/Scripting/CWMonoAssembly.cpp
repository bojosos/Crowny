#include "cwpch.h"

#include "CWMonoAssembly.h"
#include "Crowny/Common/Parser.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/appdomain.h>
#include <mono/metadata/metadata.h>
#include <mono/metadata/appdomain.h>
END_MONO_INCLUDE

namespace Crowny
{

	CWMonoAssembly::CWMonoAssembly(MonoDomain* domain, const std::string& filepath)
	{
		m_Assembly = mono_domain_assembly_open(domain, filepath.c_str());
		m_Image = mono_assembly_get_image(m_Assembly);
	}

	CWMonoClass* CWMonoAssembly::GetClass(const std::string& fullName)
	{
		auto res = SplitString(fullName, ".");
		CW_ENGINE_ASSERT(res.size() == 2, "Name has to be in the format (Namespace.ClassName)");
		return GetClass(res[0], res[1]);
	}

	CWMonoClass* CWMonoAssembly::GetClass(const std::string& namespaceName, const std::string& className)
	{
		auto it = m_Classes.find(namespaceName + "." + className);
		if (it == m_Classes.end())
		{
			MonoClass* t = mono_class_from_name(m_Image, namespaceName.c_str(), className.c_str());
			if (t)
			{
				m_Classes[namespaceName + "." + className] = new CWMonoClass(t);
				return m_Classes[namespaceName + "." + className];
			}
			else
				return nullptr;
		}
		return it->second;
	}

}

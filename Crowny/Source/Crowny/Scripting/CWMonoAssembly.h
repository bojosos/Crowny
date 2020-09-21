#pragma once

#include "CWMonoClass.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/assembly.h>
END_MONO_INCLUDE

namespace Crowny
{
	class CWMonoAssembly
	{
	public:
		CWMonoAssembly(MonoDomain* domain, const std::string& filepath);
		CWMonoClass* GetClass(const std::string& namespaceName, const std::string& className);
		CWMonoClass* GetClass(const std::string& fullName);
	private:
		MonoAssembly* m_Assembly;
		MonoImage* m_Image;
		std::unordered_map<std::string, CWMonoClass*> m_Classes;
	};
}
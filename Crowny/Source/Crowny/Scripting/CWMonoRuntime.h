#pragma once

#include "CWMonoAssembly.h"

BEGIN_MONO_INCLUDE
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
END_MONO_INCLUDE

namespace Crowny
{
	class CWMonoRuntime
	{
	public:
		
		static bool Init(const std::string& domainName);
		static void Shutdown();

		static CWMonoAssembly* LoadAssembly(const std::string& filepath);
		static CWMonoAssembly* GetAssembly(const std::string& name) { return s_Instance->m_Assembly; }
		static MonoDomain* GetDomain() { return s_Instance->m_Domain; }
		static CWMonoObject* CreateInstance(CWMonoClass* monoClass);

	private:
		static CWMonoRuntime* s_Instance;
		CWMonoAssembly* m_Assembly;
		MonoDomain* m_Domain;
	};

}
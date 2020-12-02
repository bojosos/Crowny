#pragma once

#include "Crowny/Scripting/CWMonoAssembly.h"

BEGIN_MONO_INCLUDE
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
END_MONO_INCLUDE

#define CROWNY_ASSEMBLY "Crowny.dll"
#define CLIENT_ASSEMBLY "Client.dll"

#define ASSEMBLY_COUNT        2
#define CROWNY_ASSEMBLY_INDEX 0
#define CLIENT_ASSEMBLY_INDEX 1

namespace Crowny
{
	class CWMonoRuntime
	{
	public:
		
		static bool Init(const std::string& domainName);
		static void Shutdown();

		static void LoadAssemblies(const std::string& directory);

		static CWMonoAssembly* GetCrownyAssembly() { return s_Instance->m_Assemblies[CROWNY_ASSEMBLY_INDEX]; }
		static CWMonoAssembly* GetClientAssembly() { return s_Instance->m_Assemblies[CLIENT_ASSEMBLY_INDEX]; }

		static MonoDomain* GetDomain() { return s_Instance->m_Domain; }
		static CWMonoObject* CreateInstance(CWMonoClass* monoClass);

	private:
		static CWMonoRuntime* s_Instance;
		std::vector<CWMonoAssembly*> m_Assemblies;
		MonoDomain* m_Domain;
	};

}
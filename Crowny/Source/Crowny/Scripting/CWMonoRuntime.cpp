#include "cwpch.h"

#include "CWMonoRuntime.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/threads.h>
#include <mono/utils/mono-logger.h>
#include <mono/metadata/mono-config.h>
#include <mono/metadata/debug-helpers.h>
END_MONO_INCLUDE

namespace Crowny
{
	
	CWMonoRuntime* CWMonoRuntime::s_Instance = nullptr;

	static void OnLogCallback(const char* domain, const char* level, const char* message, mono_bool, void*)
	{
		if (!domain)
			domain = "Mono";

		if (level == std::string("critical"))
			CW_ENGINE_CRITICAL("{0} -> {1}", domain, message);
		if (level == std::string("error"))
			CW_ENGINE_ERROR("{0} -> {1}", domain, message);
		if (level == std::string("warning"))
			CW_ENGINE_WARN("{0} -> {1}", domain, message);
		if (level == std::string("info"))
			CW_ENGINE_INFO("{0} -> {1}", domain, message);
		if (level == std::string("message"))
			CW_ENGINE_TRACE("{0} -> {1}", domain, message);
		if (level == std::string("debug"))
			CW_ENGINE_TRACE("{0} -> {1}", domain, message);
	}

	bool CWMonoRuntime::Init(const std::string& domainName)
	{
		s_Instance = new CWMonoRuntime();
		mono_trace_set_level_string("warning");
		mono_trace_set_log_handler(OnLogCallback, nullptr);
		s_Instance->m_Domain = mono_jit_init(domainName.c_str());
		mono_thread_set_main(mono_thread_current());
		mono_config_parse (NULL);
		CW_ENGINE_INFO("Domain {0} created!", domainName);

		return s_Instance->m_Domain != nullptr;
	}

	void CWMonoRuntime::LoadAssemblies(const std::string& directory)
	{
		s_Instance->m_Assemblies.resize(ASSEMBLY_COUNT);
		s_Instance->m_Assemblies[CROWNY_ASSEMBLY_INDEX] = new CWMonoAssembly(s_Instance->m_Domain, directory + "/" + CROWNY_ASSEMBLY);
		s_Instance->m_Assemblies[CLIENT_ASSEMBLY_INDEX] = new CWMonoAssembly(s_Instance->m_Domain, directory + "/" + CLIENT_ASSEMBLY);
		CW_ENGINE_INFO("Assembly {0} loaded", CROWNY_ASSEMBLY);
		CW_ENGINE_INFO("Assembly {0} loaded", CLIENT_ASSEMBLY);
	}

	CWMonoObject* CWMonoRuntime::CreateInstance(CWMonoClass* monoClass)
	{
		return new CWMonoObject(mono_object_new(s_Instance->m_Domain, monoClass->m_Class));
	}

	void CWMonoRuntime::Shutdown()
	{
		mono_jit_cleanup(s_Instance->m_Domain);
	}

}

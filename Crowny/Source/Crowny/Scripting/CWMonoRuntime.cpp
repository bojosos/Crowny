#include "cwpch.h"

#include "Crowny/Scripting/CWMonoRuntime.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/threads.h>
#include <mono/utils/mono-logger.h>
#include <mono/metadata/mono-config.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/appdomain.h>
END_MONO_INCLUDE

namespace Crowny
{
	
	CWMonoRuntime* CWMonoRuntime::s_Instance = nullptr;

	static void OnLogCallback(const char* domain, const char* level, const char* message, mono_bool, void*)
	{
		if (strcmp(level, "critical") == 0)
			CW_ENGINE_CRITICAL("{0} -> {1}", domain, message);
		if (strcmp(level, "error") == 0)
			CW_ENGINE_ERROR("{0} -> {1}", domain, message);
		if (strcmp(level, "warning") == 0)
			CW_ENGINE_WARN("{0} -> {1}", domain, message);
		if (strcmp(level, "info") == 0)
			CW_ENGINE_INFO("{0} -> {1}", domain, message);
		if (strcmp(level, "message") == 0)
			CW_ENGINE_INFO("{0} -> {1}", domain, message);
		if (strcmp(level, "debug") == 0)
			CW_ENGINE_INFO("{0} -> {1}", domain, message);
	}

	static void OnPrintCallback(const char* message, mono_bool isStdout)
	{
		CW_ENGINE_WARN("Mono error: {0}", message);
	}

	static void OnPrintErrorCallback(const char* message, mono_bool isStdout)
	{
		CW_ENGINE_ERROR("Mono error: {0}", message);
	}

	bool CWMonoRuntime::Init(const std::string& domainName)
	{
		s_Instance = new CWMonoRuntime();
		const char* options[] = {
			"--soft-breakpoints",
			"--debugger-agent=transport=dt_socket,address=127.0.0.1:17615,embedding=1,server=y,suspend=n",
			"--debug-domain-unload",
			"--gc-debug=check-remset-consistency,xdomain-checks"
		};

		mono_jit_parse_options(4, (char**)options);
		mono_trace_set_level_string("warning");
		mono_trace_set_log_handler(OnLogCallback, nullptr);
		mono_trace_set_print_handler(OnPrintCallback);
		mono_trace_set_printerr_handler(OnPrintErrorCallback);
		mono_config_parse (nullptr);

		s_Instance->m_Domain = mono_jit_init(domainName.c_str());
		CW_ENGINE_ASSERT(s_Instance->m_Domain, "Could not initialize Mono!");
		mono_thread_set_main(mono_thread_current());
		CW_ENGINE_INFO("Domain {0} created!", domainName);

		return s_Instance->m_Domain != nullptr;
	}

	void CWMonoRuntime::LoadAssemblies(const std::string& directory)
	{
		s_Instance->m_Assemblies.resize(ASSEMBLY_COUNT);
		s_Instance->m_Assemblies[CROWNY_ASSEMBLY_INDEX] = new CWMonoAssembly(directory + "/" + CROWNY_ASSEMBLY);
		s_Instance->m_Assemblies[CLIENT_ASSEMBLY_INDEX] = new CWMonoAssembly(directory + "/" + CLIENT_ASSEMBLY);
		s_Instance->m_Assemblies[CORLIB_ASSEMBLY_INDEX] = new CWMonoAssembly(mono_get_corlib(), "corlib");
		CW_ENGINE_INFO("Assembly {0} loaded.", CROWNY_ASSEMBLY);
		CW_ENGINE_INFO("Assembly {0} loaded.", CLIENT_ASSEMBLY);
		CW_ENGINE_INFO("Assembly {0} loaded.", CORLIB_ASSEMBLY);
		CWMonoAssembly* engine = s_Instance->m_Assemblies[CROWNY_ASSEMBLY_INDEX];
		
		s_Instance->m_BuiltinScriptClasses.SerializeFieldAttribute = engine->GetClass("Crowny", "SerializeField");
		CW_ENGINE_ASSERT(s_Instance->m_BuiltinScriptClasses.SerializeFieldAttribute != nullptr, "Assembly incomplete");
		s_Instance->m_BuiltinScriptClasses.RangeAttribute = engine->GetClass("Crowny", "Range");
		CW_ENGINE_ASSERT(s_Instance->m_BuiltinScriptClasses.RangeAttribute != nullptr, "Assembly incomplete");
		s_Instance->m_BuiltinScriptClasses.ShowInInspector = engine->GetClass("Crowny", "ShowInInspector");
		CW_ENGINE_ASSERT(s_Instance->m_BuiltinScriptClasses.ShowInInspector != nullptr, "Assembly incomplete");
		s_Instance->m_BuiltinScriptClasses.HideInInspector = engine->GetClass("Crowny", "HideInInspector");
		CW_ENGINE_ASSERT(s_Instance->m_BuiltinScriptClasses.HideInInspector != nullptr, "Assembly incomplete");
	}

	void CWMonoRuntime::Shutdown()
	{
		mono_jit_cleanup(s_Instance->m_Domain);
		mono_runtime_cleanup(s_Instance->m_Domain);
		mono_domain_free(s_Instance->m_Domain, true);
	}

}

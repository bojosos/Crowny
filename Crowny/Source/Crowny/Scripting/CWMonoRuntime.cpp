#include "cwpch.h"

#include "CWMonoRuntime.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/threads.h>
#include <mono/utils/mono-logger.h>
END_MONO_INCLUDE

namespace Crowny
{
	
	CWMonoRuntime* CWMonoRuntime::s_Instance = nullptr;

	static void OnLogCallback(const char* domain, const char* level, const char* message, mono_bool, void*)
	{
		if (level = "critical")
			CW_ENGINE_CRITICAL("{0} -> {1}", domain, message);
		if (level = "error")
			CW_ENGINE_ERROR("{0} -> {1}", domain, message);
		if (level = "warning")
			CW_ENGINE_WARN("{0} -> {1}", domain, message);
		if (level = "info")
			CW_ENGINE_INFO("{0} -> {1}", domain, message);
		if (level = "message")
			CW_ENGINE_TRACE("{0} -> {1}", domain, message);
		if (level = "debug")
			CW_ENGINE_TRACE("{0} -> {1}", domain, message);
	}

	bool CWMonoRuntime::Init(const std::string& domainName)
	{
		s_Instance = new CWMonoRuntime();
		
		mono_set_dirs("C:\\Program Files\\Mono\\lib", "C:\\Program Files\\Mono\\etc");

		mono_trace_set_level_string("warning");
		mono_trace_set_log_handler(OnLogCallback, nullptr);

		s_Instance->m_Domain = mono_jit_init(domainName.c_str());
		mono_thread_set_main(mono_thread_current());
		return s_Instance->m_Domain != nullptr;
	}

	CWMonoAssembly* CWMonoRuntime::LoadAssembly(const std::string& filepath)
	{
		s_Instance->m_Assembly = new CWMonoAssembly(s_Instance->m_Domain, filepath);
		return s_Instance->m_Assembly;
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
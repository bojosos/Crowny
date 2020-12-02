#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Logging/ScriptDebug.h"

#include "Crowny/Scripting/CWMonoRuntime.h"

namespace Crowny
{
	void ScriptDebug::InitRuntimeFunctions()
	{
		CWMonoClass* debugClass = CWMonoRuntime::GetCrownyAssembly()->GetClass("Crowny", "Debug");
		
		debugClass->AddInternalCall("Log", (void*)&Internal_Log);
		debugClass->AddInternalCall("LogWarning", (void*)&Internal_LogWarning);
		debugClass->AddInternalCall("LogError", (void*)&Internal_LogError);
		debugClass->AddInternalCall("LogException", (void*)&Internal_LogException);
	}

	void ScriptDebug::Internal_Log(MonoString* message)
	{
		const char* cstr = mono_string_to_utf8(message);
		CW_INFO(cstr);
	}

	void ScriptDebug::Internal_LogWarning(MonoString* message)
	{
		const char* cstr = mono_string_to_utf8(message);
		CW_WARN(cstr);
	}

	void ScriptDebug::Internal_LogError(MonoString* message)
	{
		const char* cstr = mono_string_to_utf8(message);
		CW_ERROR(cstr);
	}

	void ScriptDebug::Internal_LogException(MonoString* message)
	{
		const char* cstr = mono_string_to_utf8(message);
		CW_CRITICAL(cstr);
	}

}
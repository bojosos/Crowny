#include "cwpch.h"

#include "Debug.h"
#include "Crowny/Scripting/CWMonoRuntime.h"

namespace Crowny
{
	void Debug::InitRuntimeFunctions()
	{
		CWMonoClass* debugClass = CWMonoRuntime::GetAssembly("")->GetClass("Debug");

		debugClass->AddInternalCall("Internal_Log", (void*)Internal_Log);
		debugClass->AddInternalCall("Internal_LogWarning", (void*)Internal_LogWarning);
		debugClass->AddInternalCall("Internal_LogError", (void*)Internal_LogError);
		debugClass->AddInternalCall("Internal_LogException", (void*)Internal_LogException);
	}

	void Debug::Internal_Log(MonoString* message)
	{
		const char* cstr = mono_string_to_utf8(message);
		CW_INFO(cstr);
	}

	void Debug::Internal_LogWarning(MonoString* message)
	{
		const char* cstr = mono_string_to_utf8(message);
		CW_WARN(cstr);
	}

	void Debug::Internal_LogError(MonoString* message)
	{
		const char* cstr = mono_string_to_utf8(message);
		CW_ERROR(cstr);
	}

	void Debug::Internal_LogException(MonoString* message)
	{
		const char* cstr = mono_string_to_utf8(message);
		CW_CRITICAL(cstr);
	}

}
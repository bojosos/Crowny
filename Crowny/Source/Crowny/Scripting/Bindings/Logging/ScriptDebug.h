#pragma once

#include <mono/metadata/object.h>

namespace Crowny
{
	class ScriptDebug
	{
	public:
		static void InitRuntimeFunctions();

	private:
		static void Internal_Log(MonoString* message);
		static void Internal_LogWarning(MonoString* message);
		static void Internal_LogError(MonoString* message);
		static void Internal_LogException(MonoString* message);
	};
}
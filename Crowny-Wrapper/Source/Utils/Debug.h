#pragma once

#include <mono/metadata/object.h>

namespace Crowny
{
	class Debug
	{
	public:
		static void InitRuntimeFunctions();

	private:
		static void Internal_Log(MonoString* message);
		static void Internal_Warning(MonoString* message);
		static void Internal_Error(MonoString* message);
	};
}
#pragma once

#include "Crowny/Scripting/ScriptObject.h"

namespace Crowny
{
	class ScriptJSON : public ScriptObject<ScriptJSON>
	{
	public:
		SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "JsonUtility")
		ScriptJSON();

	private:
		static MonoString* Internal_ToJSON(MonoObject* object, bool prettyPrint);
		static MonoObject* Internal_FromJSON(MonoString* json, MonoReflectionType* type);
	};
} // namespace Crowny
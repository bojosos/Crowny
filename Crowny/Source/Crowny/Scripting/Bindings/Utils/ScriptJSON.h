#pragma once

#include "Crowny/Scripting/ScriptObject.h"

namespace Crowny
{
    class ScriptJson : public ScriptObject<ScriptJson>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "JsonUtility")
        ScriptJson();

    private:
        static MonoString* Internal_ToJson(MonoObject* object, bool prettyPrint);
        static MonoObject* Internal_FromJson(MonoString* json, MonoReflectionType* type);
    };
} // namespace Crowny
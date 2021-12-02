#pragma once

#include "Crowny/Scripting/ScriptObject.h"

namespace Crowny
{
    class ScriptDebug : public ScriptObject<ScriptDebug>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Debug")
        ScriptDebug();

    private:
        static void Internal_Log(MonoString* message);
        static void Internal_LogWarning(MonoString* message);
        static void Internal_LogError(MonoString* message);
        static void Internal_LogException(MonoString* message);
    };
} // namespace Crowny
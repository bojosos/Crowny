#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Logging/ScriptDebug.h"

#include <mono/metadata/object.h>

namespace Crowny
{
    ScriptDebug::ScriptDebug() : ScriptObject() {}

    void ScriptDebug::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_Log", (void*)&Internal_Log);
        MetaData.ScriptClass->AddInternalCall("Internal_LogWarning", (void*)&Internal_LogWarning);
        MetaData.ScriptClass->AddInternalCall("Internal_LogError", (void*)&Internal_LogError);
        MetaData.ScriptClass->AddInternalCall("Internal_LogException", (void*)&Internal_LogException);
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

} // namespace Crowny
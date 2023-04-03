#include "cwpch.h"

#include "Crowny/Common/ConsoleBuffer.h"
#include "Crowny/Scripting/Bindings/Logging/ScriptDebug.h"
#include "Crowny/Scripting/Mono/MonoMethod.h"

#include <mono/metadata/appdomain.h>
#include <mono/metadata/class.h>
#include <mono/metadata/metadata.h>
#include <mono/metadata/mono-debug.h>
#include <mono/metadata/object.h>
#include <mono/metadata/reflection.h>

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

    static bool Callback(::MonoMethod* method, int32_t native_offset, int32_t il_offset, bool managed, void* data)
    {
        if (managed)
        {

            ConsoleBuffer::CallstackBuffer& callstack = *((ConsoleBuffer::CallstackBuffer*)data);
            MonoMethod temp(method);
            MonoDebugMethodInfo* methodInfo = mono_debug_lookup_method(method);
            if (methodInfo)
            {
                MonoDebugSourceLocation* sourceLocation = mono_debug_method_lookup_location(methodInfo, il_offset);
                callstack.push_back({ temp.GetFullDeclName(), sourceLocation->source_file, sourceLocation->row });
                mono_debug_free_source_location(sourceLocation);
            }
        }
        return false;
    }

    void ScriptDebug::Internal_Log(MonoString* message)
    {
        ConsoleBuffer::CallstackBuffer callstack;
        MonoUtils::WalkStack(Callback, &callstack);
        const String nativeMessage = mono_string_to_utf8(message);
        ConsoleBuffer::Get().AddMessage(ConsoleBuffer::Message::Level::Info, nativeMessage, callstack);
    }

    void ScriptDebug::Internal_LogWarning(MonoString* message)
    {
        ConsoleBuffer::CallstackBuffer callstack;
        MonoUtils::WalkStack(Callback, &callstack);
        const String nativeMessage = mono_string_to_utf8(message);
        ConsoleBuffer::Get().AddMessage(ConsoleBuffer::Message::Level::Warn, nativeMessage, callstack);
    }

    void ScriptDebug::Internal_LogError(MonoString* message)
    {
        ConsoleBuffer::CallstackBuffer callstack;
        MonoUtils::WalkStack(Callback, &callstack);
        const String nativeMessage = mono_string_to_utf8(message);
        ConsoleBuffer::Get().AddMessage(ConsoleBuffer::Message::Level::Error, nativeMessage, callstack);
    }

    void ScriptDebug::Internal_LogException(MonoString* message)
    {
        ConsoleBuffer::CallstackBuffer callstack;
        MonoUtils::WalkStack(Callback, &callstack);
        const String nativeMessage = mono_string_to_utf8(message);
        ConsoleBuffer::Get().AddMessage(ConsoleBuffer::Message::Level::Critical, nativeMessage, callstack);
    }

} // namespace Crowny
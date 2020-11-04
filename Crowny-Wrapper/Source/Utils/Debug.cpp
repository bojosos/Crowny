#include "Debug.h"

namespace Crowny
{

    void Debug::InitRuntimeFunctions()
    {
        
    }

    void Debug::DebugLog(MonoString* message)
    {
        const char* cmsg = mono_string_to_utf8(string);
        CW_CLIENT_INFO(cmsg);
    }

    void Debug::DebugWarning(MonoString* message)
    {
        const char* cmsg = mono_string_to_utf8(string);
        CW_CLIENT_WARN(cmsg);
    }

    void Debug::DebugError(MonoString* message)
    {
        const char* cmsg = mono_string_to_utf8(string);
        CW_CLIENT_ERROR(cmsg);
    }

}
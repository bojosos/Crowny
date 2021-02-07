#pragma once

#include "Crowny/Scripting/CWMono.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/metadata.h>
#include <mono/metadata/object.h>
END_MONO_INCLUDE

namespace Crowny
{
    
    enum class MonoPrimitiveType
    {
        Bool,
        Char,
        I8,
        U8,
        I16,
        U16,
        I32,
        U32,
        I64,
        U64,
        R32,
        R64,
        String,
        ValueType,
        Class,
        Array,
        Generic,
        Enum,
        Unknown
    };
    
    class MonoUtils
    {
    public:
        static void CheckException(MonoException* exception);
        static void CheckException(MonoObject* exception);
        
        static bool IsEnum(MonoClass* monoClass);
        static std::string FromMonoString(MonoString* value);
        static MonoString* ToMonoString(const std::string& value);
        static MonoType* GetType(MonoClass* monoClass);
    };
}
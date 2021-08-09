#pragma once

#include "Crowny/Scripting/CWMono.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/metadata.h>
#include <mono/metadata/object.h>
END_MONO_INCLUDE

namespace Crowny
{
#undef Bool
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

        static uint32_t NewGCHandle(MonoObject* object, bool pinned);
        static void FreeGCHandle(uint32_t handle);
        static MonoObject* GetObjectFromGCHandle(uint32_t handle);

        template <class T, class... Args>
        static void InvokeThunk(T* thunk, Args... args)
        {
            MonoException* exception = nullptr;
            thunk(std::forward<Args>(args)..., &exception);
            CheckException(exception);
        }
    };
    
} // namespace Crowny

#ifdef CW_PLATFORM_WIN32
    #define CW_THUNKCALL CW_STDCALL
#else
    #define CW_THUNKCALL
#endif
#pragma once

#include "Crowny/Scripting/Mono/Mono.h"

namespace Crowny
{
    class MonoUtils
    {
    public:
        static void CheckException(MonoException* exception);
        static void CheckException(MonoObject* exception);

        static bool IsEnum(MonoClass* monoClass);
        static String FromMonoString(MonoString* value);
        static MonoString* ToMonoString(const String& value);

        static uint32_t NewGCHandle(MonoObject* object, bool pinned);
        static void FreeGCHandle(uint32_t handle);
        static MonoObject* GetObjectFromGCHandle(uint32_t handle);

        static MonoReflectionType* GetType(::MonoClass* klass);

        static void GetClassName(MonoObject* obj, String& ns, String& typeName);
        static void GetClassName(::MonoClass* obj, String& ns, String& typeName);
        static void GetClassName(MonoReflectionType* reflType, String& ns, String& typeName);

        template <class T, class... Args> static void InvokeThunk(T* thunk, Args... args)
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
#pragma once

#include "Crowny/Scripting/Mono/Mono.h"

#undef GetClassName

namespace Crowny
{
    class MonoUtils
    {
    public:
        static void CheckException(MonoException* exception);
        static void CheckException(MonoObject* exception);

        static bool IsEnum(MonoClass* monoClass);
        static String FromMonoString(MonoString* value);
        static std::wstring WFromMonoString(MonoString* value);
        static MonoString* ToMonoString(const String& value);

        static uint32_t NewGCHandle(MonoObject* object, bool pinned);
        static void FreeGCHandle(uint32_t handle);
        static MonoObject* GetObjectFromGCHandle(uint32_t handle);

        static MonoReflectionType* GetType(::MonoClass* klass);
        static ::MonoClass* GetClass(MonoObject* object);
        static ::MonoClass* GetClass(MonoReflectionType* reflType);
        static String GetReflTypeName(MonoReflectionType* type);

        static void GetClassName(MonoObject* obj, String& ns, String& typeName);
        static void GetClassName(::MonoClass* obj, String& ns, String& typeName);
        static void GetClassName(MonoReflectionType* reflType, String& ns, String& typeName);

        static bool IsValueType(::MonoClass* monoClass);

        static ::MonoClass* GetObjectClass();
        static ::MonoClass* GetBoolClass();
        static ::MonoClass* GetCharClass();
        static ::MonoClass* GetSByteClass();
        static ::MonoClass* GetByteClass();
        static ::MonoClass* GetI16Class();
        static ::MonoClass* GetU16Class();
        static ::MonoClass* GetI32Class();
        static ::MonoClass* GetU32Class();
        static ::MonoClass* GetI64Class();
        static ::MonoClass* GetU64Class();
        static ::MonoClass* GetFloatClass();
        static ::MonoClass* GetDoubleClass();
        static ::MonoClass* GetStringClass();

        static MonoPrimitiveType GetPrimitiveType(::MonoClass* monoClass);
        static MonoPrimitiveType GetEnumPrimitiveType(::MonoClass* monoClass);

        static MonoObject* Box(::MonoClass* klass, void* value);
        static void* Unbox(MonoObject* object);

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
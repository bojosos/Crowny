#include "cwpch.h"

#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"

#include <mono/metadata/class.h>
#include <mono/metadata/metadata.h>
#include <mono/metadata/object.h>
#include <mono/metadata/reflection.h>

namespace Crowny
{

    void MonoUtils::CheckException(MonoException* exception)
    {
        CheckException(reinterpret_cast<MonoObject*>(exception));
    }

    void MonoUtils::CheckException(MonoObject* exception)
    {
        if (exception != nullptr)
        {
            ::MonoClass* exceptionClass = mono_object_get_class(exception);
            const char* exceptionClassName = mono_class_get_name(exceptionClass);
            ::MonoProperty* exceptionProp = mono_class_get_property_from_name(exceptionClass, "Message");
            ::MonoMethod* exceptionMsgGetter = mono_property_get_get_method(exceptionProp);
            MonoString* exceptionMsg =
              (MonoString*)mono_runtime_invoke(exceptionMsgGetter, exception, nullptr, nullptr);

            ::MonoProperty* exceptionStackProp = mono_class_get_property_from_name(exceptionClass, "StackTrace");
            ::MonoMethod* exceptionStackGetter = mono_property_get_get_method(exceptionStackProp);
            MonoString* exceptionStackTrace =
              (MonoString*)mono_runtime_invoke(exceptionStackGetter, exception, nullptr, nullptr);

            CW_ENGINE_CRITICAL("Managed exception: {0}:  {1} ---- {2}", exceptionClassName,
                               mono_string_to_utf8(exceptionMsg),
                               mono_string_to_utf8(exceptionStackTrace)); // does this work?
        }
    }

    bool MonoUtils::IsEnum(::MonoClass* monoClass) { return mono_class_is_enum(monoClass) != 0; }

    String MonoUtils::FromMonoString(MonoString* value) { return mono_string_to_utf8(value); }

    MonoString* MonoUtils::ToMonoString(const String& value)
    {
        return mono_string_from_utf16((mono_unichar2*)value.c_str());
    }

    uint32_t MonoUtils::NewGCHandle(MonoObject* object, bool pinned) { return mono_gchandle_new(object, pinned); }

    void MonoUtils::FreeGCHandle(uint32_t handle) { mono_gchandle_free(handle); }

    MonoObject* MonoUtils::GetObjectFromGCHandle(uint32_t handle) { return mono_gchandle_get_target(handle); }

    void MonoUtils::GetClassName(MonoObject* obj, String& ns, String& typeName)
    {
        if (obj == nullptr)
            return;
        ::MonoClass* monoClass = mono_object_get_class(obj);
        GetClassName(monoClass, ns, typeName);
    }

    void MonoUtils::GetClassName(MonoReflectionType* monoReflType, String& ns, String& typeName)
    {
        MonoType* monoType = mono_reflection_type_get_type(monoReflType);
        ::MonoClass* monoClass = mono_class_from_mono_type(monoType);
        GetClassName(monoClass, ns, typeName);
    }

    void MonoUtils::GetClassName(::MonoClass* monoClass, String& ns, String& typeName)
    {
        ::MonoClass* nestingClass = mono_class_get_nesting_type(monoClass);
        if (nestingClass == nullptr)
        {
            ns = mono_class_get_namespace(monoClass);
            typeName = mono_class_get_name(monoClass);
            return;
        }
        else
        {
            const char* className = mono_class_get_name(monoClass);
            if (className)
                typeName = String("+") + className;

            do
            {
                ::MonoClass* nextNestingClass = mono_class_get_nesting_type(nestingClass);
                if (nextNestingClass != nullptr)
                {
                    typeName = String("+") + mono_class_get_name(nestingClass) + typeName;
                    nestingClass = nextNestingClass;
                }
                else
                {
                    ns = mono_class_get_namespace(nestingClass);
                    typeName = mono_class_get_name(nestingClass) + typeName;
                    break;
                }
            } while (true);
        }
    }

    MonoReflectionType* MonoUtils::GetType(::MonoClass* klass)
    {
        MonoType* type = mono_class_get_type(klass);
        return mono_type_get_object(MonoManager::Get().GetDomain(), type);
    }

} // namespace Crowny

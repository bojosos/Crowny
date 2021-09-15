#include "cwpch.h"

#include "Crowny/Scripting/MonoUtils.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/class.h>
#include <mono/metadata/metadata.h>
#include <mono/metadata/object.h>
END_MONO_INCLUDE

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
            MonoClass* exceptionClass = mono_object_get_class(exception);
            const char* exceptionClassName = mono_class_get_name(exceptionClass);
            MonoProperty* exceptionProp = mono_class_get_property_from_name(exceptionClass, "Message");
            MonoMethod* exceptionMsgGetter = mono_property_get_get_method(exceptionProp);
            MonoString* exceptionMsg =
              (MonoString*)mono_runtime_invoke(exceptionMsgGetter, exception, nullptr, nullptr);

            MonoProperty* exceptionStackProp = mono_class_get_property_from_name(exceptionClass, "StackTrace");
            MonoMethod* exceptionStackGetter = mono_property_get_get_method(exceptionStackProp);
            MonoString* exceptionStackTrace =
              (MonoString*)mono_runtime_invoke(exceptionStackGetter, exception, nullptr, nullptr);

            CW_ENGINE_CRITICAL("Managed exception: {0}:  {1} ---- {2}", exceptionClassName,
                               mono_string_to_utf8(exceptionMsg),
                               mono_string_to_utf8(exceptionStackTrace)); // does this work?
        }
    }

    bool MonoUtils::IsEnum(MonoClass* monoClass) { return mono_class_is_enum(monoClass); }

    std::string MonoUtils::FromMonoString(MonoString* value) { return mono_string_to_utf8(value); }

    MonoString* MonoUtils::ToMonoString(const std::string& value)
    {
        return mono_string_from_utf16((mono_unichar2*)value.c_str());
    }

    MonoType* MonoUtils::GetType(MonoClass* monoClass) { return mono_class_get_type(monoClass); }

    uint32_t MonoUtils::NewGCHandle(MonoObject* object, bool pinned) { return mono_gchandle_new(object, pinned); }

    void MonoUtils::FreeGCHandle(uint32_t handle) { mono_gchandle_free(handle); }

    MonoObject* MonoUtils::GetObjectFromGCHandle(uint32_t handle) { return mono_gchandle_get_target(handle); }

} // namespace Crowny

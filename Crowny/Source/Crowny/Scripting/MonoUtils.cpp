#include "cwpch.h"

#include "Crowny/Scripting/MonoUtils.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/metadata.h>
#include <mono/metadata/class.h>
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
			MonoProperty* exceptionProp = mono_class_get_property_from_name(exceptionClass, "Message");
			MonoMethod* exceptionMsgGetter = mono_property_get_get_method(exceptionProp);
			MonoString* exceptionMsg = (MonoString*)mono_runtime_invoke(exceptionMsgGetter, exception, nullptr, nullptr);
			
			MonoProperty* exceptionStackProp = mono_class_get_property_from_name(exceptionClass, "StackTrace");
			MonoMethod* exceptionStackGetter = mono_property_get_get_method(exceptionStackProp);
			MonoString* exceptionStackTrace = (MonoString*)mono_runtime_invoke(exceptionStackGetter, exception, nullptr, nullptr);
			
			CW_ENGINE_CRITICAL("Managed exception: {0}", mono_string_to_utf8(exceptionStackTrace)); // does this work?
		}
    }

}
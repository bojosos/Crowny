#include "cwpch.h"

#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"

#include "Crowny/Common/UTF8.h"

#include <mono/metadata/class.h>
#include <mono/metadata/metadata.h>
#include <mono/metadata/object.h>
#include <mono/metadata/reflection.h>
#include <mono/metadata/appdomain.h>

namespace Crowny
{

	std::wstring MonoUtils::WFromMonoString(MonoString* str)
	{
		if (str == nullptr)
			return L"";

		int len = mono_string_length(str);
		mono_unichar2* monoChars = mono_string_chars(str);

		std::wstring ret(len, '0');
		for (int i = 0; i < len; i++)
			ret[i] = monoChars[i];

		return ret;
	}

	std::string MonoUtils::FromMonoString(MonoString* str)
	{
		std::wstring wideString = WFromMonoString(str);

		return UTF8::FromWide(wideString);
	}

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

    bool MonoUtils::IsValueType(::MonoClass* monoClass) { return mono_class_is_valuetype(monoClass) != 0; }

    ::MonoClass* MonoUtils::GetClass(MonoObject* object)
    {
        return mono_object_get_class(object);
    }

    ::MonoClass* MonoUtils::GetClass(MonoReflectionType* type)
    {
        MonoType* monoType = mono_reflection_type_get_type(type);
        return mono_type_get_class(monoType);
    }

    // String MonoUtils::FromMonoString(MonoString* value) { return mono_string_to_utf8(value); }

    MonoString* MonoUtils::ToMonoString(const String& value)
    {
        // Is this right? bfs does something completely different (using wstring), but Bulgarian guy wth git-repo does this
        return mono_string_new(MonoManager::Get().GetDomain(), value.c_str());
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

    MonoObject* MonoUtils::Box(::MonoClass* klass, void* value)
    {
        return mono_value_box(MonoManager::Get().GetDomain(), klass, value);
    }

    void* MonoUtils::Unbox(MonoObject* value)
    {
        return mono_object_unbox(value);
    }

    MonoReflectionType* MonoUtils::GetType(::MonoClass* klass)
    {
        MonoType* type = mono_class_get_type(klass);
        return mono_type_get_object(MonoManager::Get().GetDomain(), type);
    }

    MonoPrimitiveType MonoUtils::GetEnumPrimitiveType(::MonoClass* monoClass)
    {
        MonoType* monoType = mono_class_get_type(monoClass);
        MonoType* underlyingType = mono_type_get_underlying_type(monoType);

        return GetPrimitiveType(mono_class_from_mono_type(underlyingType));
    }

    MonoPrimitiveType MonoUtils::GetPrimitiveType(::MonoClass* monoClass)
    {
        MonoType* type = mono_class_get_type(monoClass);
        int primitiveType = mono_type_get_type(type);
        switch (primitiveType)
        {
        case (MONO_TYPE_BOOLEAN):
            return MonoPrimitiveType::Bool;
        case (MONO_TYPE_CHAR):
            return MonoPrimitiveType::Char;
        case (MONO_TYPE_I1):
            return MonoPrimitiveType::I8;
        case (MONO_TYPE_U1):
            return MonoPrimitiveType::U8;
        case (MONO_TYPE_I2):
            return MonoPrimitiveType::I16;
        case (MONO_TYPE_U2):
            return MonoPrimitiveType::U16;
        case (MONO_TYPE_I4):
            return MonoPrimitiveType::I32;
        case (MONO_TYPE_U4):
            return MonoPrimitiveType::U32;
        case (MONO_TYPE_I8):
            return MonoPrimitiveType::I64;
        case (MONO_TYPE_U8):
            return MonoPrimitiveType::U64;
        case (MONO_TYPE_R4):
            return MonoPrimitiveType::Float;
        case (MONO_TYPE_R8):
            return MonoPrimitiveType::Double;
        case (MONO_TYPE_STRING):
            return MonoPrimitiveType::String;
        case (MONO_TYPE_CLASS):
            return MonoPrimitiveType::Class;
        case (MONO_TYPE_VALUETYPE):
            return MonoPrimitiveType::ValueType;
        case (MONO_TYPE_ARRAY):
        case (MONO_TYPE_SZARRAY):
            return MonoPrimitiveType::Array;
        case (MONO_TYPE_GENERICINST):
            return MonoPrimitiveType::Generic;
        default:
            break;
        }

        return MonoPrimitiveType::Unknown;
    }

    ::MonoClass* MonoUtils::GetObjectClass()
    {
        return mono_get_object_class();
    }

    ::MonoClass* MonoUtils::GetBoolClass()
    {
        return mono_get_boolean_class();
    }

    ::MonoClass* MonoUtils::GetCharClass()
    {
        return mono_get_char_class();
    }

    ::MonoClass* MonoUtils::GetSByteClass()
    {
        return mono_get_sbyte_class();
    }

    ::MonoClass* MonoUtils::GetByteClass()
    {
        return mono_get_byte_class();
    }

    ::MonoClass* MonoUtils::GetI16Class()
    {
        return mono_get_int16_class();
    }

    ::MonoClass* MonoUtils::GetU16Class()
    {
        return mono_get_uint16_class();
    }

    ::MonoClass* MonoUtils::GetI32Class()
    {
        return mono_get_int32_class();
    }

    ::MonoClass* MonoUtils::GetU32Class()
    {
        return mono_get_uint32_class();
    }

    ::MonoClass* MonoUtils::GetI64Class()
    {
        return mono_get_int64_class();
    }

    ::MonoClass* MonoUtils::GetU64Class()
    {
        return mono_get_uint64_class();
    }

    ::MonoClass* MonoUtils::GetFloatClass()
    {
        return mono_get_single_class();
    }

    ::MonoClass* MonoUtils::GetDoubleClass()
    {
        return mono_get_double_class();
    }

    ::MonoClass* MonoUtils::GetStringClass()
    {
        return mono_get_string_class();
    }

} // namespace Crowny

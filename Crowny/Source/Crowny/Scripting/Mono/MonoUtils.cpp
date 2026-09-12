#include "cwpch.h"

#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"

#include "Crowny/Common/UTF8.h"

#include <mono/metadata/appdomain.h>
#include <mono/metadata/class.h>
#include <mono/metadata/metadata.h>
#include <mono/metadata/mono-debug.h>
#include <mono/metadata/object.h>
#include <mono/metadata/reflection.h>

#include <utility>

namespace Crowny
{
    MonoGCHandle::MonoGCHandle(MonoObject* object, bool pinned) : m_Handle(object != nullptr ? MonoUtils::NewGCHandle(object, pinned) : 0) {}

    MonoGCHandle::~MonoGCHandle()
    {
        if (m_Handle != 0)
            MonoUtils::FreeGCHandle(m_Handle);
    }

    MonoGCHandle::MonoGCHandle(MonoGCHandle&& other) noexcept : m_Handle(std::exchange(other.m_Handle, 0)) {}

    MonoGCHandle& MonoGCHandle::operator=(MonoGCHandle&& other) noexcept
    {
        if (this != &other)
        {
            if (m_Handle != 0)
                MonoUtils::FreeGCHandle(m_Handle);
            m_Handle = std::exchange(other.m_Handle, 0);
        }
        return *this;
    }

    MonoObject* MonoGCHandle::Get() const { return m_Handle != 0 ? MonoUtils::GetObjectFromGCHandle(m_Handle) : nullptr; }

    std::wstring MonoUtils::WFromMonoString(MonoString* str)
    {
        if (str == nullptr)
            return L"";

        const int len = mono_string_length(str);
        const mono_unichar2* monoChars = mono_string_chars(str);

        std::wstring ret(len, '0');
        for (int i = 0; i < len; i++)
            ret[i] = monoChars[i];

        return ret;
    }

    std::string MonoUtils::FromMonoString(MonoString* str)
    {
        if (str == nullptr)
            return {};
        const mono_unichar2* chars = mono_string_chars(str);
        const int length = mono_string_length(str);
        return UTF8::FromUTF16(U16String(chars, chars + length));
    }

    namespace
    {
        thread_local Vector<ManagedDiagnostic> s_PendingDiagnostics;
    }

    ManagedDiagnostic MonoUtils::DescribeException(MonoObject* exception)
    {
        ManagedDiagnostic diagnostic;
        diagnostic.Code = "managed.mono.exception";
        diagnostic.Backend = ManagedBackendId::Mono;
        diagnostic.Message = "Managed exception";
        if (exception == nullptr)
            return diagnostic;

        // Getters can allocate or throw. Keep the original exception rooted and
        // capture getter failures without recursively trying to describe them.
        const uint32_t handle = NewGCHandle(exception, true);
        ::MonoClass* exceptionClass = mono_object_get_class(GetObjectFromGCHandle(handle));
        diagnostic.Message = mono_class_get_name(exceptionClass);
        const auto readProperty = [&](const char* name) -> String {
            ::MonoProperty* property = nullptr;
            for (::MonoClass* current = exceptionClass; current != nullptr && property == nullptr; current = mono_class_get_parent(current))
                property = mono_class_get_property_from_name(current, name);
            ::MonoMethod* getter = property != nullptr ? mono_property_get_get_method(property) : nullptr;
            if (getter == nullptr)
                return {};
            MonoObject* getterException = nullptr;
            MonoObject* value = mono_runtime_invoke(getter, GetObjectFromGCHandle(handle), nullptr, &getterException);
            return getterException == nullptr ? FromMonoString(reinterpret_cast<MonoString*>(value)) : String{};
        };
        const String message = readProperty("Message");
        if (!message.empty())
            diagnostic.Message += ": " + message;
        diagnostic.ManagedStack = readProperty("StackTrace");
        FreeGCHandle(handle);
        return diagnostic;
    }

    Vector<ManagedDiagnostic> MonoUtils::DrainDiagnostics()
    {
        Vector<ManagedDiagnostic> diagnostics;
        diagnostics.swap(s_PendingDiagnostics);
        return diagnostics;
    }

    void MonoUtils::CheckException(MonoException* exception) { CheckException(reinterpret_cast<MonoObject*>(exception)); }

    void MonoUtils::CheckException(MonoObject* exception)
    {
        if (exception == nullptr)
            return;
        ManagedDiagnostic diagnostic = DescribeException(exception);
        CW_ENGINE_ERROR("{}\n{}", diagnostic.Message, diagnostic.ManagedStack);
        s_PendingDiagnostics.push_back(std::move(diagnostic));
    }

    bool MonoUtils::IsEnum(MonoClass* monoClass) { return IsEnum(monoClass->GetInternalPtr()); }

    bool MonoUtils::IsEnum(::MonoClass* monoClass) { return mono_class_is_enum(monoClass) != 0; }

    bool MonoUtils::IsValueType(::MonoClass* monoClass) { return mono_class_is_valuetype(monoClass) != 0; }

    bool MonoUtils::IsSubClassOf(::MonoClass* subClass, ::MonoClass* parent) { return mono_class_is_subclass_of(subClass, parent, true) != 0; }

    ::MonoClass* MonoUtils::GetClass(MonoObject* object) { return mono_object_get_class(object); }

    ::MonoClass* MonoUtils::GetClass(MonoReflectionType* type)
    {
        MonoType* monoType = mono_reflection_type_get_type(type);
        return mono_type_get_class(monoType);
    }

    // String MonoUtils::FromMonoString(MonoString* value) { return mono_string_to_utf8(value); }

    MonoString* MonoUtils::ToMonoString(const String& value)
    {
        // Is this right? bfs does something completely different (using wstring), but Bulgarian guy wth git-repo does
        // this
        return mono_string_new(MonoManager::Get().GetDomain(), value.c_str());
    }

    uint32_t MonoUtils::NewGCHandle(MonoObject* object, bool pinned) { return mono_gchandle_new(object, pinned); }

    uint32_t MonoUtils::NewWeakGCHandle(MonoObject* object, bool trackResurrection) { return mono_gchandle_new_weakref(object, trackResurrection); }

    void MonoUtils::FreeGCHandle(uint32_t handle) { mono_gchandle_free(handle); }

    MonoObject* MonoUtils::GetObjectFromGCHandle(uint32_t handle) { return mono_gchandle_get_target(handle); }

    void MonoUtils::GetClassName(MonoObject* obj, String& ns, String& typeName)
    {
        if (obj == nullptr)
            return;
        ::MonoClass* monoClass = mono_object_get_class(obj);
        GetClassName(monoClass, ns, typeName);
    }

    String MonoUtils::GetClassName(MonoObject* object)
    {
        String name, ns;
        GetClassName(object, name, ns);
        return name + "::" + ns;
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

    MonoObject* MonoUtils::Box(::MonoClass* klass, void* value) { return mono_value_box(MonoManager::Get().GetDomain(), klass, value); }

    void* MonoUtils::Unbox(MonoObject* value) { return mono_object_unbox(value); }

    MonoReflectionType* MonoUtils::GetType(::MonoClass* klass)
    {
        MonoType* type = mono_class_get_type(klass);
        return mono_type_get_object(MonoManager::Get().GetDomain(), type);
    }

    String MonoUtils::GetReflTypeName(MonoReflectionType* reflType)
    {
        MonoType* monoType = mono_reflection_type_get_type(reflType);
        return mono_class_get_name(mono_type_get_class(monoType));
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
        const int primitiveType = mono_type_get_type(type);
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

    void MonoUtils::WalkStack(MonoStackWalk walkCallback, void* userData) { mono_stack_walk((::MonoStackWalk)walkCallback, userData); }

    ::MonoClass* MonoUtils::GetObjectClass() { return mono_get_object_class(); }

    ::MonoClass* MonoUtils::GetBoolClass() { return mono_get_boolean_class(); }

    ::MonoClass* MonoUtils::GetCharClass() { return mono_get_char_class(); }

    ::MonoClass* MonoUtils::GetSByteClass() { return mono_get_sbyte_class(); }

    ::MonoClass* MonoUtils::GetByteClass() { return mono_get_byte_class(); }

    ::MonoClass* MonoUtils::GetI16Class() { return mono_get_int16_class(); }

    ::MonoClass* MonoUtils::GetU16Class() { return mono_get_uint16_class(); }

    ::MonoClass* MonoUtils::GetI32Class() { return mono_get_int32_class(); }

    ::MonoClass* MonoUtils::GetU32Class() { return mono_get_uint32_class(); }

    ::MonoClass* MonoUtils::GetI64Class() { return mono_get_int64_class(); }

    ::MonoClass* MonoUtils::GetU64Class() { return mono_get_uint64_class(); }

    ::MonoClass* MonoUtils::GetFloatClass() { return mono_get_single_class(); }

    ::MonoClass* MonoUtils::GetDoubleClass() { return mono_get_double_class(); }

    ::MonoClass* MonoUtils::GetStringClass() { return mono_get_string_class(); }

} // namespace Crowny

#include "cwpch.h"

#include "Crowny/Scripting/Mono/MonoArray.h"
#include "Crowny/Scripting/Mono/MonoManager.h"

#include <mono/jit/jit.h>
#include <mono/metadata/object.h>

namespace Crowny
{

    namespace Detail
    {
        template <> String ScriptArray_Get<String>(MonoArray* ar, uint32_t idx)
        {
            return MonoUtils::FromMonoString(mono_array_get(ar, MonoString*, idx));
        }

        template <> Path ScriptArray_Get<Path>(MonoArray* ar, uint32_t idx)
        {
            return MonoUtils::FromMonoString(mono_array_get(ar, MonoString*, idx));
        }

        template <> void ScriptArray_Set<String>(MonoArray* ar, uint32_t idx, const String& value)
        {
            MonoString* monoString = MonoUtils::ToMonoString(value);
            mono_array_setref(ar, idx, monoString);
        }

        template <> void ScriptArray_Set<Path>(MonoArray* ar, uint32_t idx, const Path& value)
        {
            MonoString* monoString = MonoUtils::ToMonoString(value.string());
            mono_array_setref(ar, idx, monoString);
        }

        template <> void ScriptArray_Set<std::nullptr_t>(MonoArray* ar, UINT32 idx, const std::nullptr_t& value)
        {
            void** item = (void**)mono_array_addr_with_size(ar, sizeof(void*), idx);
            *item = nullptr;
        }
    } // namespace Detail

    ScriptArray::ScriptArray(::MonoArray* array) : m_Array(array) {}

    ScriptArray::ScriptArray(::MonoClass* klass, uint32_t size)
    {
        m_Array = mono_array_new(MonoManager::Get().GetDomain(), klass, size);
    }

    uint32_t ScriptArray::Size() const { return (uint32_t)mono_array_length(m_Array); }

    uint32_t ScriptArray::ElementSize() const
    {
        ::MonoClass* ar = mono_object_get_class((MonoObject*)m_Array);
        ::MonoClass* el = mono_class_get_element_class(ar);
        return (uint32_t)mono_class_array_element_size(el);
    }

    uint8_t* ScriptArray::GetRaw(uint32_t idx, uint32_t size)
    {
        return (uint8_t*)mono_array_addr_with_size(m_Array, size, idx);
    }

    void ScriptArray::Resize(uint32_t newLength)
    {
        if (newLength == 0)
            return;
        uint32_t length = Size();
        if (length == 0)
        {
            m_Array = mono_array_new(MonoManager::Get().GetDomain(), GetElementClass(), newLength);
            return;
        }
        uint32_t lengthToCopy = newLength < length ? newLength : length;
        MonoArray* temp = mono_array_new(MonoManager::Get().GetDomain(), GetElementClass(), newLength);
        if (temp == nullptr)
            return;
        // This likely doesn't work for objects
        uint32_t alignment = 0;
        uint32_t elementSize = ElementSize();
        char* src = mono_array_addr_with_size(m_Array, elementSize, 0);
        char* dst = mono_array_addr_with_size(temp, elementSize, 0);
        std::memcpy(dst, src, lengthToCopy * elementSize);
        m_Array = temp;
    }

    void ScriptArray::SetRaw(uint32_t idx, const uint8_t* value, uint32_t size, uint32_t count)
    {
        ::MonoClass* arrayClass = mono_object_get_class((MonoObject*)(m_Array));
        ::MonoClass* elementClass = mono_class_get_element_class(arrayClass);

        CW_ENGINE_ASSERT((UINT32)mono_class_array_element_size(elementClass) == size);
        CW_ENGINE_ASSERT((idx + count) <= mono_array_length(m_Array));

        if (mono_class_is_valuetype(elementClass))
            mono_value_copy_array(m_Array, idx, (void*)value, count);
        else
        {
            uint8_t* dest = (uint8_t*)mono_array_addr_with_size(m_Array, size, idx);
            mono_gc_wbarrier_arrayref_copy(dest, (void*)value, count);
        }
    }

    ::MonoClass* ScriptArray::GetElementClassGlobal(::MonoClass* arrayClass)
    {
        return mono_class_get_element_class(arrayClass);
    }

    ::MonoClass* ScriptArray::GetElementClass()
    {
		::MonoClass* ar = mono_object_get_class((MonoObject*)m_Array);
		return mono_class_get_element_class(ar);
    }

    uint8_t* ScriptArray::GetArrayAddr(MonoArray* array, uint32_t size, uint32_t idx)
    {
        return (uint8_t*)mono_array_addr_with_size(array, size, idx);
    }

    void ScriptArray::SetArrayVal(MonoArray* array, uint32_t idx, const uint8_t* value, uint32_t size, uint32_t count)
    {
        ::MonoClass* arrayClass = mono_object_get_class((MonoObject*)(array));
        ::MonoClass* elementClass = mono_class_get_element_class(arrayClass);

        CW_ENGINE_ASSERT((UINT32)mono_class_array_element_size(elementClass) == size);
        CW_ENGINE_ASSERT((idx + count) <= mono_array_length(array));

        if (mono_class_is_valuetype(elementClass))
            mono_value_copy_array(array, idx, (void*)value, count);
        else

        {
            uint8_t* dest = (uint8_t*)mono_array_addr_with_size(array, size, idx);
            mono_gc_wbarrier_arrayref_copy(dest, (void*)value, count);
        }
    }

} // namespace Crowny
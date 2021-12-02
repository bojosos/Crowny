#include "cwpch.h"

#include "Crowny/Scripting/Mono/MonoArray.h"

#include <mono/metadata/object.h>

namespace Crowny
{
    MonoArray::MonoArray(::MonoArray* array) : m_Array(array) {}

    MonoArray::MonoArray(MonoClass& array, uint32_t size)
    {
        // m_Array = mono_array_new(MonoRuntime::GetDomain());
    }

    uint32_t MonoArray::Size() { return (uint32_t)mono_array_length(m_Array); }
} // namespace Crowny
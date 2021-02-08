#include "cwpch.h"

#include "Crowny/Scripting/CWMonoArray.h"

#include "Crowny/Scripting/CWMonoRuntime.h"

namespace Crowny
{
    ScriptArray::ScriptArray(MonoArray* array) : m_Array(array)
    {
        
    }
    
    ScriptArray::ScriptArray(CWMonoClass& array, uint32_t size)
    {
        m_Array = mono_array_new(CWMonoRuntime::GetDomain())    
    }
    
    uint32_t ScriptArray::Size()
    {
        return (uint32_t)mono_array_length(m_Array);
    }
}
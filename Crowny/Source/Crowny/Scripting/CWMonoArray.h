#pragma once

#include "Crowny/Scripting/CWMonoClass.h"

#include <mono/metadata/object.h>

namespace Crowny
{
    class ScriptArray
    {
    public:
        ScriptArray(MonoArray* array);
        ScriptArray(CWMonoClass& monoClass, uint32_t size);

        template <class T>
        T Get(uint32_t idx);

        template <class T>
        void Set(uint32_t idx, const T& value);

        ~ScriptArray();

        uint32_t Size();

    private:
        MonoArray* m_Array;
    };
}
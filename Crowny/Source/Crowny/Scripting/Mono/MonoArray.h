#pragma once

#include "Crowny/Scripting/Mono/Mono.h"

#include "Crowny/Scripting/Mono/MonoClass.h"

namespace Crowny
{
    class MonoArray
    {
    public:
        MonoArray(::MonoArray* array);
        MonoArray(MonoClass& monoClass, uint32_t size);

        template <class T> T Get(uint32_t idx);

        template <class T> void Set(uint32_t idx, const T& value);

        ~MonoArray();

        uint32_t Size();

    private:
        ::MonoArray* m_Array;
    };
} // namespace Crowny
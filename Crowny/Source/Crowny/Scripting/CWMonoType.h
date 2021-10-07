#pragma once

#include "Crowny/Scripting/CWMono.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/class.h>
END_MONO_INCLUDE

namespace Crowny
{
    class CWMonoField;

    class CWMonoType
    {
    public:
        CWMonoType(MonoType* type);

        const String& GetName() const { return m_Name; }
        bool IsValueType();

        friend class CWMonoField;

    private:
        MonoType* m_Type;
        String m_Name;
    };
} // namespace Crowny
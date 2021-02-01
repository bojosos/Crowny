#pragma once

#include "Crowny/Scripting/CWMono.h"

BEGIN_MONO_INCLUDE
#include <mono/metadata/metadata.h>
END_MONO_INCLUDE

namespace Crowny
{
    class MonoUtils
    {
    public:
        static void CheckException(MonoException* exception);
        static void CheckException(MonoObject* exception);
    };
}
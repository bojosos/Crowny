#pragma once

#include <mono/metadata/object.h>

namespace Crowny
{
    class ScriptGameObject
    {
    public:
        static void InitRuntimeFunctions();

    private:
        static MonoObject* Internal_FindObject(MonoString* name);
    };

} // namespace Crowny

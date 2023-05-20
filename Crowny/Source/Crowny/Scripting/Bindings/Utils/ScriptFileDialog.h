#pragma once

#include "Crowny/Scripting/ScriptObject.h"

#include "Crowny/Utils/Compression.h"

namespace Crowny
{

    class ScriptFileDialog : public ScriptObject<ScriptFileDialog>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "FileDialog")
        ScriptFileDialog();

    private:
        static MonoString* Internal_OpenFileDialog(MonoString* title, MonoString* directory, MonoArray* filters);
        static MonoString* Internal_OpenFolderDialog(MonoString* title, MonoString* directory);
        static MonoString* Internal_SaveFileDialog(MonoString* title, MonoString* directory, MonoString* defaultName,
                                                   MonoArray* filters);
        static MonoString* Internal_SaveFolderDialog(MonoString* title, MonoString* directory, MonoString* defualtName);
    };
} // namespace Crowny

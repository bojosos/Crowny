#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Utils/ScriptFileDialog.h"
#include "Crowny/Scripting/Mono/MonoArray.h"

#include "Crowny/Common/FileSystem.h"

namespace Crowny
{
    void ScriptFileDialog::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_OpenFileDialog", &ScriptFileDialog::Internal_OpenFileDialog);
        MetaData.ScriptClass->AddInternalCall("Internal_OpenFolderDialog",
                                              &ScriptFileDialog::Internal_OpenFolderDialog);
        MetaData.ScriptClass->AddInternalCall("Internal_SaveFileDialog", &ScriptFileDialog::Internal_SaveFileDialog);
        MetaData.ScriptClass->AddInternalCall("Internal_SaveFolderDialog",
                                              &ScriptFileDialog::Internal_SaveFolderDialog);
    }

    static MonoString* CreateFileDialog(FileDialogType type, MonoString* title, MonoString* directory,
                                        MonoArray* filters)
    {
        const String titleNative = MonoUtils::FromMonoString(title);
        const String directoryNative = MonoUtils::FromMonoString(directory);
        ScriptArray array(filters);
        Vector<DialogFilter> nativeFilters;
        nativeFilters.reserve(array.Size() / 2);
        for (uint32_t i = 0; i < array.Size() / 2; i++)
        {
            MonoString* monoName = array.Get<MonoString*>(i * 2);
            MonoString* monoExtension = array.Get<MonoString*>(i * 2 + 1);
            nativeFilters.push_back({ MonoUtils::FromMonoString(monoName), MonoUtils::FromMonoString(monoExtension) });
        }

        Vector<Path> outPaths;
        if (FileSystem::OpenFileDialog(FileDialogType::OpenFile, outPaths, titleNative, directoryNative, nativeFilters))
        {
            if (outPaths.size() > 0)
            {
                MonoString* monoString = MonoUtils::ToMonoString(outPaths[0].string());
                return monoString;
            }
        }
        return nullptr;
    }

    MonoString* ScriptFileDialog::Internal_OpenFileDialog(MonoString* title, MonoString* directory, MonoArray* filters)
    {
        return CreateFileDialog(FileDialogType::OpenFile, title, directory, filters);
    }

    MonoString* ScriptFileDialog::Internal_OpenFolderDialog(MonoString* title, MonoString* directory)
    {
        return nullptr;
    }

    MonoString* ScriptFileDialog::Internal_SaveFileDialog(MonoString* title, MonoString* directory,
                                                          MonoString* defaultName, MonoArray* filters)
    {
        // return CreateFileDialog(FileDialogType::SaveFile, title, directory, defaultName, filters);
        return nullptr;
    }

    MonoString* ScriptFileDialog::Internal_SaveFolderDialog(MonoString* title, MonoString* directory,
                                                            MonoString* defualtName)
    {
        return nullptr;
    }
} // namespace Crowny
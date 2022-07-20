#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Common/PlatformUtils.h"
#include "Crowny/Common/Uuid.h"

#include <GLFW/glfw3.h>

#include <VersionHelpers.h>
#include <intrin.h>
#include <iphlpapi.h>
#include <rpc.h>
#include <shellapi.h>
#include <windows.h>
#include <ShlObj_core.h>
#include <tchar.h>

namespace Crowny
{
    UUID PlatformUtils::GenerateUUID()
    {
        ::UUID uuid;
        UuidCreate(&uuid);

        uint32_t data1 = uuid.Data1;
        uint32_t data2 = uuid.Data2 | (uuid.Data3 << 16);
        uint32_t data3 = uuid.Data3 | (uuid.Data4[0] << 16) | (uuid.Data4[1] << 24);
        uint32_t data4 = uuid.Data4[2] | (uuid.Data4[3] << 8) | (uuid.Data4[4] << 16) | (uuid.Data4[5] << 24);

        return UUID(data1, data2, data3, data4);
    }

    void PlatformUtils::ShowInExplorer(const Path& filepath)
    {
        /*const char* cmdPatter = "explorer '%s'";
        char* cmdStr = new char[filepath.string().size() + std::strlen(cmdPatter) + 1];
        std::sprintf(cmdStr, cmdPatter, filepath.c_str());

        if (system(cmdStr))
        {
        };
        delete[] cmdStr;*/
        
        if (fs::is_directory(filepath))
            ShellExecuteW(NULL, L"open", filepath.c_str(), NULL, NULL, SW_SHOWNORMAL);
        else
        {
            Path copy = filepath;
            copy.remove_filename();
		    ITEMIDLIST* dir = ILCreateFromPath(_T(copy.string().c_str()));

			ITEMIDLIST* item1 = ILCreateFromPath(_T(filepath.string().c_str()));
			const ITEMIDLIST* selection[] = { item1 };

			uint32_t count = sizeof(selection) / sizeof(ITEMIDLIST*);
            SHOpenFolderAndSelectItems(dir, count, selection, 0);
			ILFree(dir);
			ILFree(item1);
        }
    }

    void PlatformUtils::OpenExternally(const Path& filepath)
    {
        ShellExecuteW(NULL, L"open", filepath.c_str(), NULL, NULL, SW_SHOW);
        /*	const char* cmdPatter = "xdg-open '%s'";
            char* cmdStr = new char[filepath.string().size() + std::strlen(cmdPatter) + 1];
            std::sprintf(cmdStr, cmdPatter, filepath.c_str());

            if (system(cmdStr))
            {
            };
            delete[] cmdStr;*/
    }

    void PlatformUtils::CopyToClipboard(const String& string)
    {
        glfwSetClipboardString((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow(), string.c_str());
    }

    String PlatformUtils::CopyFromClipboard()
    {
        return glfwGetClipboardString((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
    }

} // namespace Crowny
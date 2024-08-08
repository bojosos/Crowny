#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Common/PlatformUtils.h"
#include "Crowny/Common/Uuid.h"
#include "Crowny/Window/Window.h"

#include <GLFW/glfw3.h>

// TODO: Remove some of these. A lot of includes for 60 lines.
#include <ShlObj_core.h>
#include <VersionHelpers.h>
#include <intrin.h>
#include <iphlpapi.h>
#include <rpc.h>
#include <shellapi.h>
#include <tchar.h>
#include <Windows.h>
#include <initguid.h>
#include <KnownFolders.h>
#include <ShlObj.h>

namespace Crowny
{
    UUID PlatformUtils::GenerateUUID()
    {
        ::UUID uuid;
        RPC_STATUS status = UuidCreate(&uuid);
        // This doesn't really matter
        if (status != RPC_S_OK)
            CW_ENGINE_CRITICAL("Couldn't create uuid");

        uint32_t data1 = uuid.Data1;
        uint32_t data2 = uuid.Data2 | (uuid.Data3 << 16);
        uint32_t data3 = uuid.Data3 | (uuid.Data4[0] << 16) | (uuid.Data4[1] << 24);
        uint32_t data4 = uuid.Data4[2] | (uuid.Data4[3] << 8) | (uuid.Data4[4] << 16) | (uuid.Data4[5] << 24);

        return UUID(data1, data2, data3, data4);
    }

    void PlatformUtils::ShowInExplorer(const Path& filepath)
    {
        if (fs::is_directory(filepath))
            ShellExecuteW(NULL, L"open", filepath.c_str(), NULL, NULL, SW_SHOWNORMAL);
        else
        {
            Path copy = filepath;
            copy.remove_filename();
            PIDLIST_ABSOLUTE dir = ILCreateFromPathW(copy.wstring().c_str());

            PIDLIST_ABSOLUTE item1 = ILCreateFromPathW(filepath.wstring().c_str());
            LPCITEMIDLIST selection[] = { item1 };

            uint32_t count = sizeof(selection) / sizeof(ITEMIDLIST*);
            SHOpenFolderAndSelectItems(dir, count, selection, 0);
            ILFree(dir);
            ILFree(item1);
        }
    }

    void PlatformUtils::OpenExternally(const Path& filepath)
    {
        ShellExecuteW(NULL, L"open", filepath.c_str(), NULL, NULL, SW_SHOW);
    }

    void PlatformUtils::CopyToClipboard(const String& string)
    {
        glfwSetClipboardString((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow(), string.c_str());
    }

    String PlatformUtils::CopyFromClipboard()
    {
        return glfwGetClipboardString((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
    }

    String PlatformUtils::Exec(const String& command)
    {
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(command.c_str(), "r"), _pclose);
        if (!pipe)
            return String();

        while (fgets(buffer.data(), (int)buffer.size(), pipe.get()) != nullptr)
            result += buffer.data();
        return result;
    }

    Path PlatformUtils::GetRoamingDirectory()
    {
        PWSTR path = nullptr;
        HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path);
        if (SUCCEEDED(hr) && path)
        {
            Path goodPath(path);
            CoTaskMemFree(path);
            return goodPath;
        }
        return Path();
    }

    const Path& PlatformUtils::GetOurRoamingDirectory()
    {
        static bool first = true;
        static const Path ourRoamingDirectory = GetRoamingDirectory() / "Crowny";
        if (first)
        {
            if (!fs::exists(ourRoamingDirectory))
                fs::create_directory(ourRoamingDirectory);
            first = false;
        }
        return ourRoamingDirectory;
    }

} // namespace Crowny
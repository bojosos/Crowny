#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Common/PlatformUtils.h"

#include <GLFW/glfw3.h>
#include <uuid/uuid.h>

namespace Crowny
{
    Uuid PlatformUtils::GenerateUuid()
    {
        uuid_t native;
        uuid_generate(native);

        return Uuid(*(uint32_t*)&native[0], *(uint32_t*)&native[4], *(uint32_t*)&native[8], *(uint32_t*)&native[12]);
    }

    void PlatformUtils::ShowInExplorer(const std::string& filepath)
    {
        const char* cmdPatter = "nautilus '%s'";
        char* cmdStr = new char[filepath.size() + std::strlen(cmdPatter) + 1];
        std::sprintf(cmdStr, cmdPatter, filepath.c_str());

        if (system(cmdStr))
        {
        };
        delete[] cmdStr;
    }

    void PlatformUtils::OpenExternally(const std::string& filepath)
    {
        const char* cmdPatter = "xdg-open '%s'";
        char* cmdStr = new char[filepath.size() + std::strlen(cmdPatter) + 1];
        std::sprintf(cmdStr, cmdPatter, filepath.c_str());

        if (system(cmdStr))
        {
        };
        delete[] cmdStr;
    }

    void PlatformUtils::CopyToClipboard(const std::string& string)
    {
        glfwSetClipboardString((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow(), string.c_str());
    }

    std::string PlatformUtils::CopyFromClipboard()
    {
        return glfwGetClipboardString((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
    }

} // namespace Crowny
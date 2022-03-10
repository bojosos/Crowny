#include "cwpch.h"
#if 0
#include "Crowny/Application/Application.h"
#include "Crowny/Common/PlatformUtils.h"

#include <GLFW/glfw3.h>
#include <uuid/uuid.h>

namespace Crowny
{
    UUID PlatformUtils::GenerateUUID()
    {
        uuid_t native;
        uuid_generate(native);

        return UUID(*(uint32_t*)&native[0], *(uint32_t*)&native[4], *(uint32_t*)&native[8], *(uint32_t*)&native[12]);
    }

    void PlatformUtils::ShowInExplorer(const Path& filepath)
    {
        const char* cmdPatter = "nautilus '%s'";
        char* cmdStr = new char[filepath.string().size() + std::strlen(cmdPatter) + 1];
        std::sprintf(cmdStr, cmdPatter, filepath.c_str());

        if (system(cmdStr))
        {
        };
        delete[] cmdStr;
    }

    void PlatformUtils::OpenExternally(const Path& filepath)
    {
        const char* cmdPatter = "xdg-open '%s'";
        char* cmdStr = new char[filepath.string().size() + std::strlen(cmdPatter) + 1];
        std::sprintf(cmdStr, cmdPatter, filepath.c_str());

        if (system(cmdStr))
        {
        };
        delete[] cmdStr;
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
#endif
#include "cwpch.h"

#ifdef CW_PLATFORM_LINUX
#include "Crowny/Application/Application.h"
#include "Crowny/Common/PlatformUtils.h"

#include <GLFW/glfw3.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <uuid/uuid.h>

namespace Crowny
{
    namespace
    {
        bool LaunchDetached(const char* executable, const Path& argument)
        {
            const String nativeArgument = argument.string();
            const pid_t child = fork();
            if (child < 0)
                return false;

            if (child == 0)
            {
                const pid_t grandchild = fork();
                if (grandchild < 0)
                    _exit(127);
                if (grandchild == 0)
                {
                    setsid();
                    execlp(executable, executable, nativeArgument.c_str(), static_cast<char*>(nullptr));
                    _exit(127);
                }
                _exit(0);
            }

            int status = 0;
            pid_t waitResult = 0;
            do
            {
                waitResult = waitpid(child, &status, 0);
            } while (waitResult < 0 && errno == EINTR);
            return waitResult == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
        }

        Path GetHomeDirectory()
        {
            if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0')
            {
                const Path homePath(home);
                if (homePath.is_absolute())
                    return homePath;
            }

            long bufferSize = sysconf(_SC_GETPW_R_SIZE_MAX);
            if (bufferSize < 0)
                bufferSize = 16384;
            Vector<char> buffer(static_cast<size_t>(bufferSize));
            passwd entry{};
            passwd* result = nullptr;
            if (getpwuid_r(getuid(), &entry, buffer.data(), buffer.size(), &result) == 0 && result != nullptr && result->pw_dir != nullptr)
                return Path(result->pw_dir);
            return {};
        }
    } // namespace

    UUID PlatformUtils::GenerateUUID()
    {
        uuid_t native;
        uuid_generate(native);
        char text[37];
        uuid_unparse_lower(native, text);
        return UUID(text);
    }

    void PlatformUtils::ShowInExplorer(const Path& filepath)
    {
        std::error_code error;
        Path target = fs::is_directory(filepath, error) ? filepath : filepath.parent_path();
        if (target.empty())
        {
            target = fs::current_path(error);
            if (error)
                return;
        }
        LaunchDetached("xdg-open", target);
    }

    void PlatformUtils::OpenExternally(const Path& filepath) { LaunchDetached("xdg-open", filepath); }

    void PlatformUtils::CopyToClipboard(const String& string)
    {
        Application* application = Application::TryGet();
        if (application == nullptr)
            return;
        glfwSetClipboardString(static_cast<GLFWwindow*>(application->GetWindow().GetNativeWindow()), string.c_str());
    }

    String PlatformUtils::CopyFromClipboard()
    {
        Application* application = Application::TryGet();
        if (application == nullptr)
            return {};
        const char* value = glfwGetClipboardString(static_cast<GLFWwindow*>(application->GetWindow().GetNativeWindow()));
        return value != nullptr ? String(value) : String();
    }

    String PlatformUtils::Exec(const String& command)
    {
        std::array<char, 256> buffer{};
        String result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
        if (!pipe)
            return result;

        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr)
            result += buffer.data();
        return result;
    }

    Path PlatformUtils::GetRoamingDirectory()
    {
        if (const char* xdgConfig = std::getenv("XDG_CONFIG_HOME"); xdgConfig != nullptr && *xdgConfig != '\0')
        {
            const Path configPath(xdgConfig);
            if (configPath.is_absolute())
                return configPath;
        }

        const Path home = GetHomeDirectory();
        if (!home.empty())
            return home / ".config";

        std::error_code error;
        return fs::temp_directory_path(error);
    }

    const Path& PlatformUtils::GetOurRoamingDirectory()
    {
        static const Path ourRoamingDirectory = []() {
            const Path roamingDirectory = GetRoamingDirectory();
            if (roamingDirectory.empty())
                return Path();
            const Path result = roamingDirectory / "Crowny";
            std::error_code error;
            fs::create_directories(result, error);
            return result;
        }();
        return ourRoamingDirectory;
    }

} // namespace Crowny
#endif

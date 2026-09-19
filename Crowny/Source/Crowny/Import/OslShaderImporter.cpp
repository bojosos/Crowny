#include "cwpch.h"

#include "Crowny/Import/ShaderImporter.h"
#include "Crowny/Renderer/ProceduralMaterialProgram.h"

#include <chrono>
#include <cstring>
#include <fstream>
#include <thread>
#ifdef CW_PLATFORM_WIN32
#include <Windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace Crowny
{
    namespace
    {
#ifdef CW_PLATFORM_WIN32
        // Quote for CreateProcess/CommandLineToArgvW, never for a command shell.
        WString QuoteArgument(const WString& value)
        {
            WString result = L"\"";
            size_t slashes = 0;
            for (wchar_t c : value)
            {
                if (c == L'\\')
                {
                    ++slashes;
                    continue;
                }
                result.append(slashes * (c == L'"' ? 2 : 1), L'\\');
                slashes = 0;
                if (c == L'"')
                    result += L'\\';
                result += c;
            }
            result.append(slashes * 2, L'\\');
            return result + L'"';
        }

        bool Cook(const Path& script, const Path& source, const Path& package, const String& output, String& error)
        {
            const wchar_t* configuredPython = _wgetenv(L"CROWNY_PYTHON");
            const WString python = configuredPython ? configuredPython : L"python.exe";
            wchar_t resolvedPython[32768]{};
            const DWORD length = SearchPathW(nullptr, python.c_str(), L".exe", 32768, resolvedPython, nullptr);
            if (length == 0 || length >= 32768)
            {
                error = "Cannot locate Python 3.9+. Set CROWNY_PYTHON to the Python executable.";
                return false;
            }
            const Path executable = resolvedPython;
            WString command = QuoteArgument(executable.wstring()) + L" " + QuoteArgument(script.wstring()) + L" " + QuoteArgument(source.wstring()) +
                              L" " + QuoteArgument(package.wstring()) + L" --output " + QuoteArgument(WString(output.begin(), output.end()));
            SECURITY_ATTRIBUTES security{ sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
            const HANDLE log = CreateFileW((package / "compiler.log").c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_ALWAYS,
                                           FILE_ATTRIBUTE_NORMAL, nullptr);
            if (log == INVALID_HANDLE_VALUE)
            {
                error = "Cannot create OSL compiler log";
                return false;
            }
            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);
            startup.dwFlags = STARTF_USESTDHANDLES;
            startup.hStdOutput = startup.hStdError = log;
            PROCESS_INFORMATION process{};
            const HANDLE job = CreateJobObjectW(nullptr, nullptr);
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            bool success = job && SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
            success = success && CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED,
                                                nullptr, nullptr, &startup, &process);
            if (success)
            {
                success = AssignProcessToJobObject(job, process.hProcess) != FALSE;
                if (success)
                {
                    ResumeThread(process.hThread);
                    success = WaitForSingleObject(process.hProcess, 120000) == WAIT_OBJECT_0;
                    if (!success)
                        error = "OSL import exceeded two minutes";
                    DWORD code = 1;
                    success = success && GetExitCodeProcess(process.hProcess, &code) && code == 0;
                }
                if (!success)
                    TerminateProcess(process.hProcess, 1);
                CloseHandle(process.hThread);
                CloseHandle(process.hProcess);
            }
            else
                error = "Cannot start OSL compiler, Windows error " + std::to_string(GetLastError());
            if (job)
                CloseHandle(job);
            CloseHandle(log);
            if (!success)
            {
                std::ifstream stream(package / "compiler.log", std::ios::binary);
                String diagnostics(32768, '\0');
                stream.read(diagnostics.data(), diagnostics.size());
                diagnostics.resize(static_cast<size_t>(stream.gcount()));
                error += "\n" + diagnostics;
            }
            return success;
        }
#else
        bool Cook(const Path& script, const Path& source, const Path& package, const String& output, String& error)
        {
            const char* configuredPython = std::getenv("CROWNY_PYTHON");
            Vector<String> arguments{
                configuredPython ? configuredPython : "python3", script.string(), source.string(), package.string(), "--output", output
            };
            Vector<char*> argv;
            for (auto& argument : arguments)
                argv.push_back(argument.data());
            argv.push_back(nullptr);
            const int log = open((package / "compiler.log").c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
            if (log < 0)
            {
                error = "Cannot create OSL compiler log";
                return false;
            }
            posix_spawn_file_actions_t actions;
            posix_spawnattr_t attributes;
            int result = posix_spawn_file_actions_init(&actions);
            const bool actionsReady = result == 0;
            if (result == 0)
                result = posix_spawn_file_actions_adddup2(&actions, log, STDOUT_FILENO);
            if (result == 0)
                result = posix_spawn_file_actions_adddup2(&actions, log, STDERR_FILENO);
            if (result == 0 && log != STDOUT_FILENO && log != STDERR_FILENO)
                result = posix_spawn_file_actions_addclose(&actions, log);
            bool attributesReady = false;
            if (result == 0)
            {
                result = posix_spawnattr_init(&attributes);
                attributesReady = result == 0;
            }
            if (result == 0)
                result = posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
            if (result == 0)
                result = posix_spawnattr_setpgroup(&attributes, 0);
            pid_t child = -1;
            if (result == 0)
                result = posix_spawnp(&child, argv[0], &actions, &attributes, argv.data(), ::environ);
            if (attributesReady)
                posix_spawnattr_destroy(&attributes);
            if (actionsReady)
                posix_spawn_file_actions_destroy(&actions);
            close(log);
            if (result != 0)
            {
                error = "Cannot start OSL compiler: " + String(std::strerror(result));
                return false;
            }
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(2);
            int status = 0;
            bool success = false;
            for (;;)
            {
                const pid_t waited = waitpid(child, &status, WNOHANG);
                if (waited == child)
                {
                    success = WIFEXITED(status) && WEXITSTATUS(status) == 0;
                    break;
                }
                if (waited < 0 && errno != EINTR)
                {
                    error = "Cannot wait for OSL compiler";
                    kill(-child, SIGKILL);
                    break;
                }
                if (std::chrono::steady_clock::now() >= deadline)
                {
                    error = "OSL import exceeded two minutes";
                    kill(-child, SIGKILL);
                    while (waitpid(child, &status, 0) < 0 && errno == EINTR)
                    {
                    }
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
            if (!success)
            {
                std::ifstream stream(package / "compiler.log", std::ios::binary);
                String diagnostics(32768, '\0');
                stream.read(diagnostics.data(), diagnostics.size());
                diagnostics.resize(static_cast<size_t>(stream.gcount()));
                error += "\n" + diagnostics;
            }
            return success;
        }
#endif
    } // namespace

    Ref<Asset> OslShaderImporter::Import(const Path& path, Ref<const ImportOptions> options)
    {
        const auto shaderOptions = DynamicRefCast<const ShaderImportOptions>(options);
        if (!shaderOptions)
            return nullptr;
        String output = "Cout";
        shaderOptions->GetDefine("CROWNY_OSL_OUTPUT", output);
        if (output.empty() || !(std::isalpha(static_cast<unsigned char>(output[0])) || output[0] == '_') ||
            !std::all_of(output.begin(), output.end(), [](unsigned char c) { return std::isalnum(c) || c == '_'; }))
        {
            CW_ENGINE_ERROR("OSL output must be a parameter identifier: '{}'", output);
            return nullptr;
        }
        Path script;
#ifdef CW_PLATFORM_WIN32
        const wchar_t* configured = _wgetenv(L"CROWNY_OSL_IMPORT_SCRIPT");
#else
        const char* configured = std::getenv("CROWNY_OSL_IMPORT_SCRIPT");
#endif
        if (configured)
            script = configured;
        else
        {
            script = std::filesystem::absolute("Tools/osl/editor-import.py");
            if (!std::filesystem::is_regular_file(script))
                script = std::filesystem::absolute("../Tools/osl/editor-import.py");
        }
        if (!std::filesystem::is_regular_file(script))
        {
            CW_ENGINE_ERROR("OSL importer is not configured. Set CROWNY_OSL_IMPORT_SCRIPT to Tools/osl/editor-import.py.");
            return nullptr;
        }
        const Path package = std::filesystem::temp_directory_path() / ("crowny-osl-" + UuidGenerator::Generate().ToString());
        struct Cleanup
        {
            Path Directory;
            ~Cleanup()
            {
                std::error_code ignored;
                std::filesystem::remove_all(Directory, ignored);
            }
        } cleanup{ package };
        try
        {
            std::filesystem::create_directory(package);
            String error;
            if (!Cook(std::filesystem::absolute(script), std::filesystem::absolute(path), package, output, error))
            {
                CW_ENGINE_ERROR("OSL import failed for '{}': {}", path, error);
                return nullptr;
            }
            const auto shader = ProceduralMaterialProgram::LoadShader(package, error);
            if (!shader)
                CW_ENGINE_ERROR("Invalid OSL package for '{}': {}", path, error);
            return shader;
        }
        catch (const std::exception& exception)
        {
            CW_ENGINE_ERROR("OSL import failed for '{}': {}", path, exception.what());
        }
        return nullptr;
    }
} // namespace Crowny

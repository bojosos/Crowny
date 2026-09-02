#include "cwepch.h"

#ifdef CW_PLATFORM_WIN32
#include <Windows.h>
#elif !defined(CW_EMSCRIPTEN)
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "Editor/Script/ScriptProjectGenerator.h"
#include "Editor/Script/VSCodeEditor.h"

#include "Crowny/Common/FileSystem.h"

#include <algorithm>
#include <array>
#include <sstream>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

namespace Crowny
{
    namespace
    {
        using rapidjson::Document;
        using rapidjson::SizeType;
        using rapidjson::StringBuffer;
        using rapidjson::Value;

        constexpr const char* CSHARP_EXTENSION = "ms-dotnettools.csharp";
        constexpr const char* CSHARP_DEV_KIT_EXTENSION = "ms-dotnettools.csdevkit";
        constexpr const char* CROWNY_ATTACH_CONFIGURATION = "Attach to Crowny (.NET)";

#ifdef CW_PLATFORM_WIN32
        WString QuoteArgument(const WString& value) { return L"\"" + value + L"\""; }

        Path GetWindowsEnvironmentPath(const wchar_t* variable)
        {
            std::array<wchar_t, 32768> value{};
            const DWORD length = GetEnvironmentVariableW(variable, value.data(), static_cast<DWORD>(value.size()));
            if (length == 0 || length >= value.size())
                return {};
            return Path(WString(value.data(), length));
        }

        Path FindExecutableOnPath(const wchar_t* executable)
        {
            std::array<wchar_t, 32768> value{};
            const DWORD length = SearchPathW(nullptr, executable, nullptr, static_cast<DWORD>(value.size()), value.data(), nullptr);
            if (length == 0 || length >= value.size())
                return {};
            return Path(WString(value.data(), length));
        }
#elif !defined(CW_EMSCRIPTEN)
#ifdef CW_MACOSX
        Path GetEnvironmentPath(const char* variable)
        {
            const char* value = std::getenv(variable);
            return value == nullptr ? Path() : Path(value);
        }
#endif

        Path FindExecutableOnPath(const char* executable)
        {
            const char* pathValue = std::getenv("PATH");
            if (pathValue == nullptr)
                return {};

            const String paths(pathValue);
            size_t start = 0;
            while (start <= paths.size())
            {
                const size_t end = paths.find(':', start);
                const String directory = paths.substr(start, end == String::npos ? String::npos : end - start);
                const Path candidate = (directory.empty() ? Path(".") : Path(directory)) / executable;
                if (access(candidate.c_str(), X_OK) == 0)
                    return candidate;
                if (end == String::npos)
                    break;
                start = end + 1;
            }

            return {};
        }
#endif

        void AddInstallation(Vector<CodeEditorInstallation>& installations, const Path& executablePath, const char* name, bool prerelease)
        {
            std::error_code error;
            if (!fs::is_regular_file(executablePath, error))
                return;

            const Path normalized = executablePath.lexically_normal();
            const bool alreadyAdded = std::any_of(installations.begin(), installations.end(), [&](const CodeEditorInstallation& installation) {
                return installation.ExecutablePath.lexically_normal() == normalized;
            });
            if (!alreadyAdded)
                installations.push_back({ normalized, prerelease, name, CodeEditorVersion::VSCode });
        }

#ifdef CW_MACOSX
        void AddMacApplication(Vector<CodeEditorInstallation>& installations, const Path& applicationPath, const char* name, bool prerelease)
        {
            std::error_code error;
            if (!fs::is_directory(applicationPath, error))
                return;

            const Path normalized = applicationPath.lexically_normal();
            const bool alreadyAdded = std::any_of(installations.begin(), installations.end(), [&](const CodeEditorInstallation& installation) {
                return installation.ExecutablePath.lexically_normal() == normalized;
            });
            if (!alreadyAdded)
                installations.push_back({ normalized, prerelease, name, CodeEditorVersion::VSCode });
        }
#endif

        bool ReadJsonObject(const Path& path, Document& document)
        {
            std::error_code error;
            if (!fs::exists(path, error))
            {
                document.SetObject();
                return true;
            }

            const String contents = FileSystem::ReadTextFile(path);
            document.Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag>(contents.c_str());
            if (document.HasParseError() || !document.IsObject())
            {
                CW_ENGINE_WARN("Cannot update {} because it is not a JSON object.", path.string());
                return false;
            }
            return true;
        }

        bool WriteJsonObject(const Path& path, const Document& document)
        {
            StringBuffer buffer;
            rapidjson::PrettyWriter<StringBuffer> writer(buffer);
            writer.SetIndent(' ', 4);
            document.Accept(writer);

            String contents(buffer.GetString(), buffer.GetSize());
            contents += '\n';
            std::error_code readError;
            if (fs::is_regular_file(path, readError) && FileSystem::ReadTextFile(path) == contents)
                return true;

            String error;
            if (FileSystem::WriteTextFileAtomic(path, contents, &error))
                return true;

            CW_ENGINE_WARN("Could not write {}: {}", path.string(), error);
            return false;
        }

        Value* GetOrCreateObject(Document& document, const char* name)
        {
            auto member = document.FindMember(name);
            if (member == document.MemberEnd())
            {
                Value key;
                key.SetString(name, document.GetAllocator());
                Value value(rapidjson::kObjectType);
                document.AddMember(key.Move(), value.Move(), document.GetAllocator());
                member = document.FindMember(name);
            }

            if (!member->value.IsObject())
                return nullptr;
            return &member->value;
        }

        Value* GetOrCreateArray(Document& document, const char* name)
        {
            auto member = document.FindMember(name);
            if (member == document.MemberEnd())
            {
                Value key;
                key.SetString(name, document.GetAllocator());
                Value value(rapidjson::kArrayType);
                document.AddMember(key.Move(), value.Move(), document.GetAllocator());
                member = document.FindMember(name);
            }

            if (!member->value.IsArray())
                return nullptr;
            return &member->value;
        }

        bool SetString(Document& document, const char* name, const String& value)
        {
            auto member = document.FindMember(name);
            if (member == document.MemberEnd())
            {
                Value key;
                key.SetString(name, document.GetAllocator());
                Value string;
                string.SetString(value.c_str(), static_cast<SizeType>(value.size()), document.GetAllocator());
                document.AddMember(key.Move(), string.Move(), document.GetAllocator());
                return true;
            }

            if (!member->value.IsString())
                return false;
            member->value.SetString(value.c_str(), static_cast<SizeType>(value.size()), document.GetAllocator());
            return true;
        }

        bool AddStringToArray(Document& document, Value& array, const char* value)
        {
            for (const Value& entry : array.GetArray())
            {
                if (entry.IsString() && String(entry.GetString(), entry.GetStringLength()) == value)
                    return true;
            }

            Value entry;
            entry.SetString(value, document.GetAllocator());
            array.PushBack(entry.Move(), document.GetAllocator());
            return true;
        }

        bool ConfigureVSCodeExtensions(const Path& directory)
        {
            Document document;
            const Path path = directory / "extensions.json";
            if (!ReadJsonObject(path, document))
                return false;

            Value* recommendations = GetOrCreateArray(document, "recommendations");
            if (recommendations == nullptr)
            {
                CW_ENGINE_WARN("Cannot update {} because recommendations is not an array.", path.string());
                return false;
            }

            AddStringToArray(document, *recommendations, CSHARP_EXTENSION);
            AddStringToArray(document, *recommendations, CSHARP_DEV_KIT_EXTENSION);
            return WriteJsonObject(path, document);
        }

        bool ConfigureVSCodeSettings(const Path& directory, const String& solutionName)
        {
            Document document;
            const Path path = directory / "settings.json";
            if (!ReadJsonObject(path, document))
                return false;

            if (!SetString(document, "dotnet.defaultSolution", solutionName + ".sln"))
            {
                CW_ENGINE_WARN("Cannot update {} because dotnet.defaultSolution is not a string.", path.string());
                return false;
            }

            Value* excludes = GetOrCreateObject(document, "files.exclude");
            if (excludes == nullptr)
            {
                CW_ENGINE_WARN("Cannot update {} because files.exclude is not an object.", path.string());
                return false;
            }
            if (excludes->FindMember("**/Internal/Assemblies/**") == excludes->MemberEnd())
            {
                Value key;
                key.SetString("**/Internal/Assemblies/**", document.GetAllocator());
                excludes->AddMember(key.Move(), true, document.GetAllocator());
            }

            return WriteJsonObject(path, document);
        }

        bool ConfigureCoreClrLaunch(const Path& directory)
        {
            Document document;
            const Path path = directory / "launch.json";
            if (!ReadJsonObject(path, document))
                return false;

            Value* configurations = GetOrCreateArray(document, "configurations");
            if (configurations == nullptr)
            {
                CW_ENGINE_WARN("Cannot update {} because configurations is not an array.", path.string());
                return false;
            }

            for (const Value& configuration : configurations->GetArray())
            {
                if (!configuration.IsObject())
                    continue;
                const auto name = configuration.FindMember("name");
                if (name != configuration.MemberEnd() && name->value.IsString() &&
                    String(name->value.GetString(), name->value.GetStringLength()) == CROWNY_ATTACH_CONFIGURATION)
                {
                    return true;
                }
            }

            Value configuration(rapidjson::kObjectType);
            Value name;
            name.SetString(CROWNY_ATTACH_CONFIGURATION, document.GetAllocator());
            configuration.AddMember("name", name.Move(), document.GetAllocator());
            configuration.AddMember("type", "coreclr", document.GetAllocator());
            configuration.AddMember("request", "attach", document.GetAllocator());
            configuration.AddMember("processId", "${command:pickProcess}", document.GetAllocator());
            configurations->PushBack(configuration.Move(), document.GetAllocator());
            return WriteJsonObject(path, document);
        }

        bool ConfigureVSCodeWorkspace(const CodeSolutionData& data, const Path& solutionPath)
        {
            const Path directory = solutionPath / ".vscode";
            std::error_code error;
            fs::create_directories(directory, error);
            if (error)
            {
                CW_ENGINE_WARN("Could not create VS Code settings directory {}: {}", directory.string(), error.message());
                return false;
            }

            if (fs::is_regular_file(directory / ".crowny-vscode-patch-disable", error))
                return true;

            const bool extensionsConfigured = ConfigureVSCodeExtensions(directory);
            const bool settingsConfigured = ConfigureVSCodeSettings(directory, data.Name);

            const bool hasCoreClrProject = std::any_of(data.Projects.begin(), data.Projects.end(), [](const CodeProjectData& project) {
                return project.Runtime == CSharpProjectRuntime::CoreCLR;
            });
            const bool launchConfigured = !hasCoreClrProject || ConfigureCoreClrLaunch(directory);
            return extensionsConfigured && settingsConfigured && launchConfigured;
        }

        Path FindWorkspace(const Path& solutionPath)
        {
            std::error_code error;
            Vector<Path> workspaces;
            for (const fs::directory_entry& entry : fs::directory_iterator(solutionPath, error))
            {
                if (error)
                    break;
                if (entry.is_regular_file(error) && entry.path().extension() == ".code-workspace")
                    workspaces.push_back(entry.path());
            }
            return workspaces.size() == 1 ? workspaces.front() : solutionPath;
        }

        bool StartVSCode(const Path& executable, const Path& workspace, const Path& filePath, uint32_t line)
        {
#ifdef CW_PLATFORM_WIN32
            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);
            PROCESS_INFORMATION process{};

            std::wstringstream arguments;
            arguments << QuoteArgument(executable.wstring()) << L" --reuse-window " << QuoteArgument(workspace.wstring());
            if (!filePath.empty())
            {
                const uint32_t targetLine = std::max(1u, line);
                const WString target = filePath.wstring() + L":" + std::to_wstring(targetLine) + L":1";
                arguments << L" --goto " << QuoteArgument(target);
            }

            WString commandLine = arguments.str();
            std::error_code error;
            const Path workingDirectory = fs::is_directory(workspace, error) ? workspace : workspace.parent_path();
            if (CreateProcessW(executable.c_str(), commandLine.data(), nullptr, nullptr, false, 0, nullptr, workingDirectory.c_str(), &startup,
                               &process))
            {
                CloseHandle(process.hProcess);
                CloseHandle(process.hThread);
                return true;
            }

            CW_ENGINE_WARN("Could not start Visual Studio Code (error {}).", GetLastError());
            return false;
#elif !defined(CW_EMSCRIPTEN)
            Vector<String> arguments = { executable.string(), "--reuse-window", workspace.string() };
            if (!filePath.empty())
            {
                const uint32_t targetLine = std::max(1u, line);
                arguments.push_back("--goto");
                arguments.push_back(filePath.string() + ":" + std::to_string(targetLine) + ":1");
            }

            const pid_t launcher = fork();
            if (launcher < 0)
            {
                CW_ENGINE_WARN("Could not start Visual Studio Code: {}", std::strerror(errno));
                return false;
            }
            if (launcher == 0)
            {
                const pid_t editor = fork();
                if (editor == 0)
                {
#ifdef CW_MACOSX
                    if (fs::is_directory(executable))
                    {
                        Vector<String> openArguments = { "open", "-n", executable.string(), "--args" };
                        openArguments.insert(openArguments.end(), arguments.begin() + 1, arguments.end());
                        Vector<char*> applicationArguments;
                        for (String& argument : openArguments)
                            applicationArguments.push_back(argument.data());
                        applicationArguments.push_back(nullptr);
                        execv("/usr/bin/open", applicationArguments.data());
                    }
                    else
#endif
                    {
                        Vector<char*> applicationArguments;
                        for (String& argument : arguments)
                            applicationArguments.push_back(argument.data());
                        applicationArguments.push_back(nullptr);
                        execv(executable.c_str(), applicationArguments.data());
                    }
                    _exit(127);
                }
                _exit(0);
            }

            int status = 0;
            while (waitpid(launcher, &status, 0) == -1 && errno == EINTR)
            {
            }
            return true;
#else
            CW_ENGINE_WARN("Visual Studio Code integration is unavailable on this platform.");
            return false;
#endif
        }
    } // namespace

    VSCodeEditor::VSCodeEditor(Path executablePath) : m_ExecutablePath(std::move(executablePath)) {}

    void VSCodeEditor::OpenFile(const Path& solutionPath, const Path& filePath, uint32_t line) const
    {
        if (m_ExecutablePath.empty())
            return;
        StartVSCode(m_ExecutablePath, FindWorkspace(solutionPath), filePath, line);
    }

    CodeEditorSyncResult VSCodeEditor::Sync(const CodeSolutionData& data, const Path& solutionPath) const
    {
        bool changed = false;
        if (!CSProject::WriteSolution(CSProjectVersion::VS2026, data, solutionPath, &changed))
            return {};
        if (!ConfigureVSCodeWorkspace(data, solutionPath))
            return {};
        return { true, changed };
    }

    void VSCodeEditor::SetEditorExecutablePath(const Path& path) { m_ExecutablePath = path; }

    void VSCodeEditor::ReloadSolution(const CodeSolutionData& data, const Path& solutionPath) const {}

    VSCodeEditorFactory::VSCodeEditorFactory()
    {
#ifdef CW_PLATFORM_WIN32
        const Path localAppData = GetWindowsEnvironmentPath(L"LOCALAPPDATA");
        const Path programFiles = GetWindowsEnvironmentPath(L"ProgramFiles");
        const Path programFilesX86 = GetWindowsEnvironmentPath(L"ProgramFiles(x86)");
        Vector<Path> roots = { programFiles, programFilesX86 };
        if (!localAppData.empty())
            roots.push_back(localAppData / "Programs");
        for (const Path& root : roots)
        {
            if (root.empty())
                continue;
            AddInstallation(m_Installations, root / "Microsoft VS Code" / "Code.exe", "Visual Studio Code", false);
            AddInstallation(m_Installations, root / "Microsoft VS Code Insiders" / "Code - Insiders.exe", "Visual Studio Code - Insiders", true);
        }

        AddInstallation(m_Installations, FindExecutableOnPath(L"Code.exe"), "Visual Studio Code", false);
        AddInstallation(m_Installations, FindExecutableOnPath(L"Code - Insiders.exe"), "Visual Studio Code - Insiders", true);
#elif defined(CW_MACOSX)
        const Path homeDirectory = GetEnvironmentPath("HOME");
        Vector<Path> applicationRoots = { Path("/Applications") };
        if (!homeDirectory.empty())
            applicationRoots.push_back(homeDirectory / "Applications");
        for (const Path& root : applicationRoots)
        {
            if (root.empty())
                continue;
            AddMacApplication(m_Installations, root / "Visual Studio Code.app", "Visual Studio Code", false);
            AddMacApplication(m_Installations, root / "Visual Studio Code - Insiders.app", "Visual Studio Code - Insiders", true);
        }

        AddInstallation(m_Installations, FindExecutableOnPath("code"), "Visual Studio Code", false);
        AddInstallation(m_Installations, FindExecutableOnPath("code-insiders"), "Visual Studio Code - Insiders", true);
#elif !defined(CW_EMSCRIPTEN)
        const Vector<Path> stableCandidates = { Path("/usr/bin/code"), Path("/bin/code"), Path("/usr/local/bin/code") };
        const Vector<Path> insidersCandidates = { Path("/usr/bin/code-insiders"), Path("/bin/code-insiders"), Path("/usr/local/bin/code-insiders") };
        for (const Path& candidate : stableCandidates)
            AddInstallation(m_Installations, candidate, "Visual Studio Code", false);
        for (const Path& candidate : insidersCandidates)
            AddInstallation(m_Installations, candidate, "Visual Studio Code - Insiders", true);

        AddInstallation(m_Installations, FindExecutableOnPath("code"), "Visual Studio Code", false);
        AddInstallation(m_Installations, FindExecutableOnPath("code-insiders"), "Visual Studio Code - Insiders", true);
#endif
    }

    CodeEditor* VSCodeEditorFactory::Create(const Path& path) const
    {
        const auto installation = std::find_if(m_Installations.begin(), m_Installations.end(),
                                               [&](const CodeEditorInstallation& candidate) { return candidate.ExecutablePath == path; });
        return installation == m_Installations.end() ? nullptr : new VSCodeEditor(installation->ExecutablePath);
    }
} // namespace Crowny

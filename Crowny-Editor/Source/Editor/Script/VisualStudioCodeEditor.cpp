#include "cwepch.h"

#ifdef CW_PLATFORM_WIN32

// Uncomment to generate the dte80a.tlh file
// #pragma warning(disable: 4278)
// #import "libid:80cc9f66-e7d8-4ddd-85b6-d9e6cd0e93e2" version("8.0") lcid("0") raw_interfaces_only named_guids
// #pragma warning(default: 4278)
#include "dte80a.tlh"

// Keep at top because of ambiguous UUID
#include <Windows.h>
#include <wrl/client.h>
#undef UUID

#include "Editor/Script/CodeEditor.h"
#include "Editor/Script/ScriptProjectGenerator.h"
#include "Editor/Script/VisualStudioCodeEditor.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/PlatformUtils.h"
#include "Crowny/Common/StringUtils.h"

#include <rapidjson/document.h>

namespace Crowny
{
    using Microsoft::WRL::ComPtr;

    constexpr uint32_t RETRY_INTERVAL_MS = 100; // Wait 100ms between retry
    constexpr uint32_t TIMEOUT_MS = 10000;      // Wait for 10s

    inline static WString QuoteString(const WString& str) { return L"\"" + str + L"\""; }

    static Path FindToolInAncestors(Path directory, const Path& relativePath)
    {
        std::error_code error;
        directory = fs::absolute(directory, error);
        if (error)
            return {};

        while (!directory.empty())
        {
            const Path candidate = directory / relativePath;
            if (fs::is_regular_file(candidate, error))
                return candidate;
            error.clear();

            const Path parent = directory.parent_path();
            if (parent == directory)
                break;
            directory = parent;
        }
        return {};
    }

    static Path FindVsWhere()
    {
        const Path relativePath = Path("3rdparty") / "vswhere" / "vswhere.exe";

        std::error_code error;
        Path result = FindToolInAncestors(fs::current_path(error), relativePath);
        if (!result.empty())
            return result;

        std::array<wchar_t, 32768> executablePath{};
        const DWORD executablePathLength = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
        if (executablePathLength > 0 && executablePathLength < executablePath.size())
        {
            result = FindToolInAncestors(Path(WString(executablePath.data(), executablePathLength)).parent_path(), relativePath);
            if (!result.empty())
                return result;
        }

        std::array<wchar_t, MAX_PATH> programFiles{};
        const DWORD programFilesLength = GetEnvironmentVariableW(L"ProgramFiles(x86)", programFiles.data(), static_cast<DWORD>(programFiles.size()));
        if (programFilesLength > 0 && programFilesLength < programFiles.size())
        {
            const Path installedPath =
              Path(WString(programFiles.data(), programFilesLength)) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe";
            if (fs::is_regular_file(installedPath, error))
                return installedPath;
        }

        return {};
    }

    class ScopedBstr
    {
    public:
        ScopedBstr() = default;
        explicit ScopedBstr(const wchar_t* value) : m_Value(SysAllocString(value)) {}
        ~ScopedBstr() { SysFreeString(m_Value); }

        ScopedBstr(const ScopedBstr&) = delete;
        ScopedBstr& operator=(const ScopedBstr&) = delete;

        BSTR Get() const { return m_Value; }
        BSTR* Put()
        {
            SysFreeString(m_Value);
            m_Value = nullptr;
            return &m_Value;
        }

    private:
        BSTR m_Value = nullptr;
    };

    struct VSProjectInfo
    {
        WString GUID;
        WString Name;
        Path path;
    };

    static String ErrorCodeToMsg(DWORD error)
    {
        CW_ENGINE_ASSERT(error != 0);
        LPSTR messageBuffer = nullptr;

        size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error,
                                     MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

        std::string message(messageBuffer, size);

        LocalFree(messageBuffer);
        return message;
    }

    class VSMessageFilter : public IMessageFilter
    {
        DWORD __stdcall HandleInComingCall(DWORD dwCallType, HTASK htaskCaller, DWORD dwTickCount, LPINTERFACEINFO lpInterfaceInfo) override
        {
            return SERVERCALL_ISHANDLED;
        }

        DWORD __stdcall RetryRejectedCall(HTASK htaskCallee, DWORD dwTickCount, DWORD dwRejectType) override
        {
            if ((dwRejectType == SERVERCALL_RETRYLATER || dwRejectType == SERVERCALL_REJECTED) && dwTickCount < TIMEOUT_MS)
                return RETRY_INTERVAL_MS;

            if (dwRejectType == SERVERCALL_RETRYLATER)
                return 99;
            return -1;
        }

        DWORD __stdcall MessagePending(HTASK htaskCallee, DWORD dwTickCount, DWORD dwPendingType) override { return PENDINGMSG_WAITDEFPROCESS; }

        HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) override
        {
            if (riid == IID_IMessageFilter || riid == IID_IUnknown)
            {
                AddRef();
                *ppvObject = this;
                return S_OK;
            }
            else
            {
                *ppvObject = nullptr;
                return E_NOINTERFACE;
            }
        }

        ULONG __stdcall AddRef() override { return InterlockedIncrement(&m_RefCount); }

        ULONG __stdcall Release() override
        {
            LONG count = InterlockedDecrement(&m_RefCount);
            if (count == 0)
            {
                delete this;
                return 0;
            }
            else
                return count;
        }

    private:
        LONG m_RefCount = 1;
    };

    class VisualStudio
    {
    public:
        static ComPtr<EnvDTE::_DTE> FindRunningInstance(const Path& solutionPath, const Path& vsExePath)
        {
            ComPtr<IRunningObjectTable> runningObjectTable;
            if (FAILED(GetRunningObjectTable(0, runningObjectTable.ReleaseAndGetAddressOf())))
                return nullptr;

            ComPtr<IEnumMoniker> enumMoniker;
            if (FAILED(runningObjectTable->EnumRunning(enumMoniker.ReleaseAndGetAddressOf())))
                return nullptr;

            ComPtr<IMoniker> moniker;
            ULONG count = 0;
            while (enumMoniker->Next(1, moniker.ReleaseAndGetAddressOf(), &count) == S_OK && count)
            {
                ComPtr<IUnknown> curObject;
                HRESULT result = runningObjectTable->GetObject(moniker.Get(), curObject.ReleaseAndGetAddressOf());
                moniker.Reset();

                if (result != S_OK)
                    continue;

                ComPtr<EnvDTE::_DTE> dte;
                curObject.As(&dte);

                if (dte == nullptr)
                    continue;

                ComPtr<EnvDTE::_Solution> solution;
                if (FAILED(dte->get_Solution(solution.ReleaseAndGetAddressOf())))
                    continue;

                ScopedBstr vsFullName;
                if (FAILED(dte->get_FullName(vsFullName.Put())))
                    continue;
                Path curPath = WString(vsFullName.Get());
                ScopedBstr solutionName;
                if (FAILED(solution->get_FullName(solutionName.Put())))
                    continue;
                Path curSolPath = WString(solutionName.Get());
                if (curSolPath.empty())
                    continue;

                if (fs::equivalent(curSolPath, solutionPath))
                {
                    if (!vsExePath.empty())
                    {
                        if (fs::equivalent(curPath, vsExePath))
                            CW_ENGINE_WARN("The running Visual studio instance does not seem to be the version "
                                           "requested in the user prefs.");
                    }
                    else
                        CW_ENGINE_WARN("Visual Studio version not selected in user prefs. Using the running version");

                    return dte;
                }
            }
            return nullptr;
        }

        static ComPtr<EnvDTE::_DTE> CreateInstance(const CLSID& clsID, const Path& solutionPath)
        {
            ComPtr<EnvDTE::_DTE> dte;
            if (FAILED(
                  ::CoCreateInstance(clsID, nullptr, CLSCTX_LOCAL_SERVER, EnvDTE::IID__DTE, reinterpret_cast<void**>(dte.ReleaseAndGetAddressOf()))))
                return nullptr;

            dte->put_UserControl(VARIANT_TRUE);

            ComPtr<EnvDTE::_Solution> solution;
            if (FAILED(dte->get_Solution(solution.ReleaseAndGetAddressOf())))
                return nullptr;

            WString widePath = solutionPath.wstring();
            ScopedBstr bstrSolution(widePath.c_str());
            if (FAILED(solution->Open(bstrSolution.Get())))
                return nullptr;

            uint32_t elapsed = 0;
            while (elapsed < TIMEOUT_MS)
            {
                ComPtr<EnvDTE::Window> window;
                if (SUCCEEDED(dte->get_MainWindow(window.ReleaseAndGetAddressOf())))
                    return dte;
                Sleep(RETRY_INTERVAL_MS);
                elapsed += RETRY_INTERVAL_MS;
            }

            return nullptr;
        }

        static bool OpenFile(const ComPtr<EnvDTE::_DTE>& dte, const Path& filePath, uint32_t line)
        {
            ComPtr<EnvDTE::ItemOperations> itemOperations;
            if (FAILED(dte->get_ItemOperations(itemOperations.ReleaseAndGetAddressOf())))
                return false;

            WString widePath = filePath.wstring();

            ScopedBstr bstrFilePath(widePath.c_str());
            ScopedBstr bstrKind(L"{00000000-0000-0000-0000-000000000000}");
            ComPtr<EnvDTE::Window> window;
            if (FAILED(itemOperations->OpenFile(bstrFilePath.Get(), bstrKind.Get(), window.ReleaseAndGetAddressOf())))
                return false;
            ComPtr<EnvDTE::Document> activeDocument;
            if (line > 0 && SUCCEEDED(dte->get_ActiveDocument(activeDocument.ReleaseAndGetAddressOf())))
            {
                ComPtr<IDispatch> selection;
                if (SUCCEEDED(activeDocument->get_Selection(selection.ReleaseAndGetAddressOf())))
                {
                    ComPtr<EnvDTE::TextSelection> textSelection;
                    if (selection != nullptr && SUCCEEDED(selection.As(&textSelection)))
                    {
                        textSelection->GotoLine(line, VARIANT_TRUE);
                        textSelection->EndOfLine(false);
                    }
                }
            }

            window.Reset();
            if (SUCCEEDED(dte->get_MainWindow(window.ReleaseAndGetAddressOf())))
            {
                window->Activate();

                HWND hWnd;
                window->get_HWnd((LONG*)&hWnd);
                SetForegroundWindow(hWnd);
            }
            return true;
        }

        static bool StartVisualStudioProcess(const Path& vsExePath, const Path& solutionPath, DWORD& processId)
        {
            STARTUPINFOW si;
            PROCESS_INFORMATION pi;
            BOOL result;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));

            std::wstring startingDirectory = vsExePath.parent_path();
            std::wstringstream commandLineStream;
            commandLineStream << QuoteString(vsExePath) << L" ";
            commandLineStream << QuoteString(solutionPath);

            WString commandLine = commandLineStream.str();
            result = CreateProcessW(vsExePath.c_str(), commandLine.data(), nullptr, nullptr, false, 0, nullptr, startingDirectory.c_str(), &si, &pi);
            if (!result)
            {
                DWORD error = GetLastError();
                CW_ENGINE_ERROR("Starting Visual Studio process failed: {}", ErrorCodeToMsg(error));
                return false;
            }
            processId = pi.dwProcessId;
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return true;
        }

        static void ReloadSolution(const Path& solutionPath, const Path& editorPath)
        {
            ComPtr<EnvDTE::_DTE> dte = VisualStudio::FindRunningInstance(solutionPath, editorPath);
            // Only try and reload the solution if we have a running visual studio instance.
            if (dte == nullptr)
            {
                return;
            }
            ComPtr<EnvDTE::_Solution> solution;
            if (FAILED(dte->get_Solution(solution.ReleaseAndGetAddressOf())))
                return;

            if (!SUCCEEDED(solution->Close(false)))
                return;
            ScopedBstr bstrSolution(solutionPath.c_str());
            if (!SUCCEEDED(solution->Open(bstrSolution.Get())))
                CW_ENGINE_WARN("Couldn't reopen solution.");
        }
    };

    VisualStudioCodeEditor::VisualStudioCodeEditor(VisualStudioVersion version, const Path& execPath) : m_Version(version), m_ExecPath(execPath) {}

    void VisualStudioCodeEditor::OpenFile(const Path& solutionPath, const Path& filePath, uint32_t line) const
    {
        if (!SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)))
        {
            CW_ENGINE_WARN("Couldn't initialize COM");
            return;
        }

        ComPtr<EnvDTE::_DTE> dte = VisualStudio::FindRunningInstance(solutionPath, m_ExecPath);
        if (dte == nullptr)
        {
            DWORD processId = 0;
            if (!VisualStudio::StartVisualStudioProcess(m_ExecPath, solutionPath, processId))
                return;
            int timeWaited = 0;
            int TIMEOUT_MS = 90;
            while (timeWaited < TIMEOUT_MS)
            {
                dte = VisualStudio::FindRunningInstance(solutionPath, m_ExecPath);
                if (dte != nullptr)
                    break;
                Sleep(RETRY_INTERVAL_MS);
                timeWaited += RETRY_INTERVAL_MS;
            }
        }

        if (dte == nullptr)
        {
            CoUninitialize();
            return;
        }

        VSMessageFilter* newFilter = new VSMessageFilter();
        IMessageFilter* oldFilter;

        CoRegisterMessageFilter(newFilter, &oldFilter);
        ComPtr<EnvDTE::Window> window;
        if (SUCCEEDED(dte->get_MainWindow(window.ReleaseAndGetAddressOf())))
            window->Activate();

        VisualStudio::OpenFile(dte, filePath, line);
        CoRegisterMessageFilter(oldFilter, nullptr);

        window.Reset();
        dte.Reset();
        CoUninitialize();
    }

    void VisualStudioCodeEditor::Sync(const CodeSolutionData& data, const Path& solutionPath) const
    {
        CSProjectVersion csProjectVersion;
        switch (m_Version)
        {
        case VisualStudioVersion::VS2008:
            csProjectVersion = CSProjectVersion::VS2008;
            break;
        case VisualStudioVersion::VS2010:
            csProjectVersion = CSProjectVersion::VS2010;
            break;
        case VisualStudioVersion::VS2012:
            csProjectVersion = CSProjectVersion::VS2012;
            break;
        case VisualStudioVersion::VS2013:
            csProjectVersion = CSProjectVersion::VS2013;
            break;
        case VisualStudioVersion::VS2015:
            csProjectVersion = CSProjectVersion::VS2015;
            break;
        case VisualStudioVersion::VS2017:
            csProjectVersion = CSProjectVersion::VS2017;
            break;
        case VisualStudioVersion::VS2019:
            csProjectVersion = CSProjectVersion::VS2019;
            break;
        case VisualStudioVersion::VS2022:
            csProjectVersion = CSProjectVersion::VS2022;
            break;
        }

        String solutionString = CSProject::GenerateSolution(csProjectVersion, data);
        solutionString = StringUtils::Replace(solutionString, "\n", "\n\r");
        Path solutionPathCopy = solutionPath;
        solutionPathCopy = solutionPath / (data.Name + ".sln");

        for (const CodeProjectData& project : data.Projects)
        {
            String projectString = CSProject::GenerateProject(csProjectVersion, project);
            projectString = StringUtils::Replace(projectString, "\n", "\n\r");

            const Path projectPath = solutionPath / (project.Name + ".csproj");

            Ref<DataStream> projectStream = FileSystem::CreateAndOpenFile(projectPath);
            projectStream->Write(projectString.c_str(), projectString.size() * sizeof(String::value_type));
            projectStream->Close();
        }

        Ref<DataStream> solutionStream = FileSystem::CreateAndOpenFile(solutionPathCopy);
        solutionStream->Write(solutionString.c_str(), solutionString.size() * sizeof(String::value_type));
        solutionStream->Close();
    }

    void VisualStudioCodeEditor::ReloadSolution(const CodeSolutionData& data, const Path& solutionPath) const
    {
        Path solutionPathCopy = solutionPath;
        solutionPathCopy = solutionPath / (data.Name + ".sln");
        VisualStudio::ReloadSolution(solutionPathCopy, m_ExecPath);
    }

    void VisualStudioCodeEditor::SetEditorExecutablePath(const Path& path) { m_ExecPath = path; }

    VisualStudioCodeEditorFactory::VisualStudioCodeEditorFactory()
    {
        Map<String, CodeEditorVersion> vsVersions = {
            { "2008", CodeEditorVersion::VS2008 }, { "2010", CodeEditorVersion::VS2010 }, { "2012", CodeEditorVersion::VS2012 },
            { "2013", CodeEditorVersion::VS2013 }, { "2015", CodeEditorVersion::VS2015 }, { "2017", CodeEditorVersion::VS2017 },
            { "2019", CodeEditorVersion::VS2019 }, { "2022", CodeEditorVersion::VS2022 },
        };
        using namespace rapidjson;

        const Path vswherePath = FindVsWhere();
        if (vswherePath.empty())
        {
            CW_ENGINE_WARN("Could not find vswhere.exe; Visual Studio integration is unavailable.");
            return;
        }

        const String vswhereCommand = "\"" + vswherePath.string() + "\" -prerelease -format json -utf8";
        String jsonResult = PlatformUtils::Exec(vswhereCommand);
        Document document;
        document.Parse(jsonResult);
        if (document.HasParseError() || !document.IsArray())
        {
            CW_ENGINE_WARN("vswhere.exe returned invalid installation data.");
            return;
        }
        for (const Value& val : document.GetArray())
        {
            const bool isPrerelease = val.FindMember("isPrerelease")->value.GetBool();
            const Path productPath = val.FindMember("productPath")->value.GetString();
            const String displayName = val.FindMember("displayName")->value.GetString();
            const auto& catalog = val.FindMember("catalog")->value;
            const String displayVersion = catalog.FindMember("productDisplayVersion")->value.GetString();
            const String name = displayName + " [" + displayVersion + "]";
            const String versionString = catalog.FindMember("productLineVersion")->value.GetString();
            const CodeEditorVersion version = vsVersions[versionString]; // TODO: get this
            m_SupportedEditors.push_back({ productPath, isPrerelease, name, version });
        }
    }

    CodeEditor* VisualStudioCodeEditorFactory::Create(const Path& executablePath) const
    {
        Map<CodeEditorVersion, VisualStudioVersion> versionData = {
            { CodeEditorVersion::VS2008, VisualStudioVersion::VS2008 }, { CodeEditorVersion::VS2010, VisualStudioVersion::VS2010 },
            { CodeEditorVersion::VS2012, VisualStudioVersion::VS2012 }, { CodeEditorVersion::VS2013, VisualStudioVersion::VS2013 },
            { CodeEditorVersion::VS2015, VisualStudioVersion::VS2015 }, { CodeEditorVersion::VS2017, VisualStudioVersion::VS2017 },
            { CodeEditorVersion::VS2019, VisualStudioVersion::VS2019 }, { CodeEditorVersion::VS2022, VisualStudioVersion::VS2022 },
        };
        for (const CodeEditorInstallation& install : m_SupportedEditors)
        {
            if (install.ExecutablePath == executablePath)
                return new VisualStudioCodeEditor(versionData[install.Version], executablePath);
        }
        return nullptr;
    }

} // namespace Crowny
#endif

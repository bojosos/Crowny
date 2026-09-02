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

#include <algorithm>
#include <cstdint>

#include <rapidjson/document.h>

namespace Crowny
{
    using Microsoft::WRL::ComPtr;

    constexpr uint32_t COM_RETRY_INTERVAL_MS = 100;
    constexpr uint32_t COM_CALL_TIMEOUT_MS = 10000;

    WString QuoteArgument(const WString& value)
    {
        WString result = L"\"";
        size_t slashCount = 0;
        for (const wchar_t character : value)
        {
            if (character == L'\\')
            {
                ++slashCount;
                continue;
            }

            if (character == L'\"')
                result.append(slashCount * 2 + 1, L'\\');
            else
                result.append(slashCount, L'\\');
            result += character;
            slashCount = 0;
        }

        result.append(slashCount * 2, L'\\');
        result += L'\"';
        return result;
    }

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

    static String FormatWindowsError(DWORD error)
    {
        LPSTR messageBuffer = nullptr;
        const DWORD size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error,
                                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&messageBuffer), 0, nullptr);
        if (size == 0 || messageBuffer == nullptr)
            return fmt::format("Win32 error 0x{:08X}", error);

        String message(messageBuffer, size);
        LocalFree(messageBuffer);
        while (!message.empty() && (message.back() == '\r' || message.back() == '\n' || message.back() == ' '))
            message.pop_back();
        return message;
    }

    static bool PathsReferToSameFile(const Path& lhs, const Path& rhs)
    {
        if (lhs.empty() || rhs.empty())
            return false;

        std::error_code error;
        if (fs::equivalent(lhs, rhs, error))
            return true;

        error.clear();
        Path normalizedLhs = fs::weakly_canonical(lhs, error);
        if (error)
        {
            error.clear();
            normalizedLhs = fs::absolute(lhs, error);
        }
        if (error)
            normalizedLhs = lhs;

        error.clear();
        Path normalizedRhs = fs::weakly_canonical(rhs, error);
        if (error)
        {
            error.clear();
            normalizedRhs = fs::absolute(rhs, error);
        }
        if (error)
            normalizedRhs = rhs;

        const WString lhsText = normalizedLhs.lexically_normal().wstring();
        const WString rhsText = normalizedRhs.lexically_normal().wstring();
        return CompareStringOrdinal(lhsText.c_str(), -1, rhsText.c_str(), -1, TRUE) == CSTR_EQUAL;
    }

    class VSMessageFilter : public IMessageFilter
    {
    public:
        DWORD __stdcall HandleInComingCall(DWORD dwCallType, HTASK htaskCaller, DWORD dwTickCount, LPINTERFACEINFO lpInterfaceInfo) override
        {
            return SERVERCALL_ISHANDLED;
        }

        DWORD __stdcall RetryRejectedCall(HTASK htaskCallee, DWORD dwTickCount, DWORD dwRejectType) override
        {
            if (dwRejectType == SERVERCALL_RETRYLATER && dwTickCount < COM_CALL_TIMEOUT_MS)
                return COM_RETRY_INTERVAL_MS;
            return static_cast<DWORD>(-1);
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

    class ComApartmentScope
    {
    public:
        ComApartmentScope() : m_Result(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)) {}

        ~ComApartmentScope()
        {
            if (SUCCEEDED(m_Result))
                CoUninitialize();
        }

        bool IsUsable() const { return SUCCEEDED(m_Result) || m_Result == RPC_E_CHANGED_MODE; }
        bool SupportsMessageFilter() const { return SUCCEEDED(m_Result); }
        HRESULT Result() const { return m_Result; }

    private:
        HRESULT m_Result;
    };

    class MessageFilterScope
    {
    public:
        explicit MessageFilterScope(bool supported)
        {
            if (!supported)
                return;

            VSMessageFilter* filter = new VSMessageFilter();
            const HRESULT result = CoRegisterMessageFilter(filter, &m_Previous);
            filter->Release();
            if (SUCCEEDED(result))
            {
                m_Registered = true;
                return;
            }

            if (m_Previous != nullptr)
            {
                m_Previous->Release();
                m_Previous = nullptr;
            }
            CW_ENGINE_WARN("Could not register the Visual Studio COM retry filter: {}", FormatWindowsError(static_cast<DWORD>(result)));
        }

        ~MessageFilterScope()
        {
            if (m_Registered)
            {
                IMessageFilter* replacedFilter = nullptr;
                const HRESULT result = CoRegisterMessageFilter(m_Previous, &replacedFilter);
                if (replacedFilter != nullptr)
                    replacedFilter->Release();
                if (FAILED(result))
                    CW_ENGINE_WARN("Could not restore the previous Visual Studio COM retry filter: {}",
                                   FormatWindowsError(static_cast<DWORD>(result)));
            }

            if (m_Previous != nullptr)
                m_Previous->Release();
        }

        MessageFilterScope(const MessageFilterScope&) = delete;
        MessageFilterScope& operator=(const MessageFilterScope&) = delete;

    private:
        IMessageFilter* m_Previous = nullptr;
        bool m_Registered = false;
    };

    class VisualStudio
    {
    public:
        static ComPtr<EnvDTE::_DTE> FindRunningInstance(const Path& solutionPath, const Path& vsExePath, DWORD expectedProcessId = 0)
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
                if (FAILED(curObject.As(&dte)) || dte == nullptr)
                    continue;

                ComPtr<EnvDTE::_Solution> solution;
                if (FAILED(dte->get_Solution(solution.ReleaseAndGetAddressOf())))
                    continue;

                ScopedBstr solutionName;
                if (FAILED(solution->get_FullName(solutionName.Put())) || solutionName.Get() == nullptr)
                    continue;
                Path curSolPath = WString(solutionName.Get());
                if (curSolPath.empty() || !PathsReferToSameFile(curSolPath, solutionPath))
                    continue;

                if (!vsExePath.empty())
                {
                    ScopedBstr runningExecutable;
                    if (FAILED(dte->get_FullName(runningExecutable.Put())) || runningExecutable.Get() == nullptr ||
                        !PathsReferToSameFile(Path(WString(runningExecutable.Get())), vsExePath))
                        continue;
                }

                if (expectedProcessId != 0)
                {
                    const DWORD runningProcessId = GetProcessId(dte);
                    if (runningProcessId != 0 && runningProcessId != expectedProcessId)
                        continue;
                }

                return dte;
            }
            return nullptr;
        }

        static DWORD GetProcessId(const ComPtr<EnvDTE::_DTE>& dte)
        {
            ComPtr<EnvDTE::Window> mainWindow;
            if (FAILED(dte->get_MainWindow(mainWindow.ReleaseAndGetAddressOf())) || mainWindow == nullptr)
                return 0;

            LONG windowValue = 0;
            if (FAILED(mainWindow->get_HWnd(&windowValue)) || windowValue == 0)
                return 0;

            DWORD processId = 0;
            GetWindowThreadProcessId(reinterpret_cast<HWND>(static_cast<intptr_t>(windowValue)), &processId);
            return processId;
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

            if (window != nullptr)
                window->Activate();

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

            ComPtr<EnvDTE::Window> mainWindow;
            if (SUCCEEDED(dte->get_MainWindow(mainWindow.ReleaseAndGetAddressOf())) && mainWindow != nullptr)
            {
                mainWindow->Activate();

                LONG windowValue = 0;
                if (SUCCEEDED(mainWindow->get_HWnd(&windowValue)) && windowValue != 0)
                    SetForegroundWindow(reinterpret_cast<HWND>(static_cast<intptr_t>(windowValue)));
            }
            return true;
        }

        static bool StartVisualStudioProcess(const Path& vsExePath, const Path& solutionPath, DWORD& processId)
        {
            std::error_code error;
            if (!fs::is_regular_file(vsExePath, error))
            {
                CW_ENGINE_WARN("Visual Studio executable does not exist: {}", vsExePath.string());
                return false;
            }
            error.clear();
            if (!fs::is_regular_file(solutionPath, error))
            {
                CW_ENGINE_WARN("Visual Studio solution does not exist: {}", solutionPath.string());
                return false;
            }

            STARTUPINFOW si;
            PROCESS_INFORMATION pi;
            BOOL result;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));

            std::wstring startingDirectory = vsExePath.parent_path();
            std::wstringstream commandLineStream;
            commandLineStream << QuoteArgument(vsExePath.wstring()) << L" ";
            commandLineStream << QuoteArgument(solutionPath.wstring());

            WString commandLine = commandLineStream.str();
            result = CreateProcessW(vsExePath.c_str(), commandLine.data(), nullptr, nullptr, false, 0, nullptr, startingDirectory.c_str(), &si, &pi);
            if (!result)
            {
                DWORD error = GetLastError();
                CW_ENGINE_ERROR("Starting Visual Studio process failed: {}", FormatWindowsError(error));
                return false;
            }
            processId = pi.dwProcessId;
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return true;
        }

        static bool ReloadSolution(const Path& solutionPath, const Path& editorPath)
        {
            ComPtr<EnvDTE::_DTE> dte = VisualStudio::FindRunningInstance(solutionPath, editorPath);
            if (dte == nullptr)
                return false;

            ComPtr<EnvDTE::_Solution> solution;
            if (FAILED(dte->get_Solution(solution.ReleaseAndGetAddressOf())))
                return false;

            VARIANT_BOOL saved = VARIANT_FALSE;
            if (FAILED(solution->get_Saved(&saved)))
            {
                CW_ENGINE_WARN("Could not determine whether Visual Studio has unsaved solution changes; skipping automatic reload.");
                return false;
            }
            if (saved == VARIANT_FALSE)
            {
                CW_ENGINE_WARN("Visual Studio has unsaved solution changes; skipping automatic reload.");
                return false;
            }

            if (FAILED(solution->Close(false)))
            {
                CW_ENGINE_WARN("Could not close Visual Studio solution before reload.");
                return false;
            }
            const WString solutionText = solutionPath.wstring();
            ScopedBstr bstrSolution(solutionText.c_str());
            if (FAILED(solution->Open(bstrSolution.Get())))
            {
                CW_ENGINE_WARN("Could not reopen Visual Studio solution.");
                return false;
            }
            return true;
        }
    };

    VisualStudioCodeEditor::VisualStudioCodeEditor(VisualStudioVersion version, const Path& execPath) : m_Version(version), m_ExecPath(execPath) {}

    void VisualStudioCodeEditor::OpenFile(const Path& solutionPath, const Path& filePath, uint32_t line) const
    {
        ComApartmentScope apartment;
        if (!apartment.IsUsable())
        {
            CW_ENGINE_WARN("Could not initialize COM for Visual Studio automation: {}", FormatWindowsError(static_cast<DWORD>(apartment.Result())));
            return;
        }
        MessageFilterScope messageFilter(apartment.SupportsMessageFilter());

        ComPtr<EnvDTE::_DTE> dte = VisualStudio::FindRunningInstance(solutionPath, m_ExecPath);
        if (dte == nullptr)
        {
            DWORD processId = 0;
            if (!VisualStudio::StartVisualStudioProcess(m_ExecPath, solutionPath, processId))
                return;

            uint32_t timeWaited = 0;
            while (timeWaited < COM_CALL_TIMEOUT_MS)
            {
                dte = VisualStudio::FindRunningInstance(solutionPath, m_ExecPath, processId);
                if (dte != nullptr)
                    break;
                Sleep(COM_RETRY_INTERVAL_MS);
                timeWaited += COM_RETRY_INTERVAL_MS;
            }
        }

        if (dte == nullptr)
        {
            CW_ENGINE_WARN("Visual Studio did not expose automation for {} within {} ms.", solutionPath.string(), COM_CALL_TIMEOUT_MS);
            return;
        }

        if (!VisualStudio::OpenFile(dte, filePath, line))
            CW_ENGINE_WARN("Visual Studio could not open {}.", filePath.string());
    }

    CodeEditorSyncResult VisualStudioCodeEditor::Sync(const CodeSolutionData& data, const Path& solutionPath) const
    {
        CSProjectVersion csProjectVersion = CSProjectVersion::VS2022;
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
        case VisualStudioVersion::VS2026:
            csProjectVersion = CSProjectVersion::VS2026;
            break;
        }

        const bool hasCoreClrProject = std::any_of(data.Projects.begin(), data.Projects.end(),
                                                   [](const CodeProjectData& project) { return project.Runtime == CSharpProjectRuntime::CoreCLR; });
        if (hasCoreClrProject && m_Version != VisualStudioVersion::VS2022 && m_Version != VisualStudioVersion::VS2026)
        {
            CW_ENGINE_WARN("CoreCLR script projects target .NET 10. Open this solution with Visual Studio 2022 or newer and the matching .NET SDK.");
        }

        bool changed = false;
        if (!CSProject::WriteSolution(csProjectVersion, data, solutionPath, &changed))
            return {};
        return { true, changed };
    }

    void VisualStudioCodeEditor::ReloadSolution(const CodeSolutionData& data, const Path& solutionPath) const
    {
        ComApartmentScope apartment;
        if (!apartment.IsUsable())
        {
            CW_ENGINE_WARN("Could not initialize COM for Visual Studio solution reload: {}",
                           FormatWindowsError(static_cast<DWORD>(apartment.Result())));
            return;
        }
        MessageFilterScope messageFilter(apartment.SupportsMessageFilter());

        const Path generatedSolutionPath = solutionPath / (data.Name + ".sln");
        VisualStudio::ReloadSolution(generatedSolutionPath, m_ExecPath);
    }

    void VisualStudioCodeEditor::SetEditorExecutablePath(const Path& path) { m_ExecPath = path; }

    namespace
    {
        CodeEditorVersion GetCodeEditorVersion(const String& productLineVersion)
        {
            static const Map<String, CodeEditorVersion> versions = {
                { "2008", CodeEditorVersion::VS2008 }, { "2010", CodeEditorVersion::VS2010 }, { "2012", CodeEditorVersion::VS2012 },
                { "2013", CodeEditorVersion::VS2013 }, { "2015", CodeEditorVersion::VS2015 }, { "2017", CodeEditorVersion::VS2017 },
                { "2019", CodeEditorVersion::VS2019 }, { "2022", CodeEditorVersion::VS2022 }, { "2026", CodeEditorVersion::VS2026 },
            };
            const auto version = versions.find(productLineVersion);
            if (version != versions.end())
                return version->second;

            CW_ENGINE_WARN("Visual Studio {} is not explicitly listed. Using the Visual Studio 2026 project format.", productLineVersion);
            return CodeEditorVersion::VS2026;
        }

        VisualStudioVersion GetVisualStudioVersion(CodeEditorVersion version)
        {
            switch (version)
            {
            case CodeEditorVersion::VS2008:
                return VisualStudioVersion::VS2008;
            case CodeEditorVersion::VS2010:
                return VisualStudioVersion::VS2010;
            case CodeEditorVersion::VS2012:
                return VisualStudioVersion::VS2012;
            case CodeEditorVersion::VS2013:
                return VisualStudioVersion::VS2013;
            case CodeEditorVersion::VS2015:
                return VisualStudioVersion::VS2015;
            case CodeEditorVersion::VS2017:
                return VisualStudioVersion::VS2017;
            case CodeEditorVersion::VS2019:
                return VisualStudioVersion::VS2019;
            case CodeEditorVersion::VS2022:
                return VisualStudioVersion::VS2022;
            case CodeEditorVersion::VS2026:
                return VisualStudioVersion::VS2026;
            case CodeEditorVersion::VSCode:
            case CodeEditorVersion::MonoDevelop:
            case CodeEditorVersion::None:
                return VisualStudioVersion::VS2022;
            }

            return VisualStudioVersion::VS2022;
        }

        bool ReadVisualStudioRegistryValue(const wchar_t* key, const wchar_t* valueName, WString& value)
        {
            DWORD byteCount = 0;
            const LONG sizeStatus =
              RegGetValueW(HKEY_LOCAL_MACHINE, key, valueName, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, nullptr, nullptr, &byteCount);
            if (sizeStatus != ERROR_SUCCESS || byteCount < sizeof(wchar_t))
                return false;

            Vector<wchar_t> buffer(byteCount / sizeof(wchar_t));
            const LONG readStatus =
              RegGetValueW(HKEY_LOCAL_MACHINE, key, valueName, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, nullptr, buffer.data(), &byteCount);
            if (readStatus != ERROR_SUCCESS)
                return false;

            value = buffer.data();
            return !value.empty();
        }

        void AddLegacyVisualStudioInstallations(Vector<CodeEditorInstallation>& installations)
        {
            struct LegacyVisualStudio
            {
                const wchar_t* RegistryVersion;
                CodeEditorVersion Version;
                const char* Name;
            };
            static const std::array<LegacyVisualStudio, 6> versions = {{
                { L"9.0", CodeEditorVersion::VS2008, "Visual Studio 2008" },  { L"10.0", CodeEditorVersion::VS2010, "Visual Studio 2010" },
                { L"11.0", CodeEditorVersion::VS2012, "Visual Studio 2012" }, { L"12.0", CodeEditorVersion::VS2013, "Visual Studio 2013" },
                { L"14.0", CodeEditorVersion::VS2015, "Visual Studio 2015" }, { L"15.0", CodeEditorVersion::VS2017, "Visual Studio 2017" },
            }};
            static const std::array<const wchar_t*, 2> registryKeys = {
                L"SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VS7",
                L"SOFTWARE\\WOW6432Node\\Microsoft\\VisualStudio\\SxS\\VS7",
            };

            for (const wchar_t* registryKey : registryKeys)
            {
                for (const LegacyVisualStudio& version : versions)
                {
                    WString installationDirectory;
                    if (!ReadVisualStudioRegistryValue(registryKey, version.RegistryVersion, installationDirectory))
                        continue;

                    const Path root = installationDirectory;
                    const Vector<Path> candidates = { root / "devenv.exe", root / "Common7" / "IDE" / "devenv.exe" };
                    for (const Path& candidate : candidates)
                    {
                        std::error_code error;
                        if (!fs::is_regular_file(candidate, error))
                            continue;

                        const Path executablePath = candidate.lexically_normal();
                        const bool alreadyAdded =
                          std::any_of(installations.begin(), installations.end(), [&](const CodeEditorInstallation& installation) {
                              return installation.ExecutablePath.lexically_normal() == executablePath;
                          });
                        if (!alreadyAdded)
                            installations.push_back({ executablePath, false, version.Name, version.Version });
                        break;
                    }
                }
            }
        }
    } // namespace

    VisualStudioCodeEditorFactory::VisualStudioCodeEditorFactory()
    {
        using namespace rapidjson;

        const Path vswherePath = FindVsWhere();
        if (vswherePath.empty())
        {
            CW_ENGINE_WARN("Could not find vswhere.exe. Searching the legacy Visual Studio registry entries.");
        }
        else
        {
            const String vswhereCommand = "\"" + vswherePath.string() + "\" -prerelease -format json -utf8";
            String jsonResult = PlatformUtils::Exec(vswhereCommand);
            Document document;
            document.Parse(jsonResult);
            if (document.HasParseError() || !document.IsArray())
            {
                CW_ENGINE_WARN("vswhere.exe returned invalid installation data.");
            }
            else
            {
                for (const Value& val : document.GetArray())
                {
                    if (!val.IsObject())
                        continue;

                    const auto productPath = val.FindMember("productPath");
                    const auto displayName = val.FindMember("displayName");
                    const auto catalogMember = val.FindMember("catalog");
                    if (productPath == val.MemberEnd() || !productPath->value.IsString() || displayName == val.MemberEnd() ||
                        !displayName->value.IsString() || catalogMember == val.MemberEnd() || !catalogMember->value.IsObject())
                    {
                        CW_ENGINE_WARN("vswhere.exe returned an incomplete Visual Studio installation record.");
                        continue;
                    }

                    const auto& catalog = catalogMember->value;
                    const auto displayVersion = catalog.FindMember("productDisplayVersion");
                    const auto productLineVersion = catalog.FindMember("productLineVersion");
                    if (displayVersion == catalog.MemberEnd() || !displayVersion->value.IsString() || productLineVersion == catalog.MemberEnd() ||
                        !productLineVersion->value.IsString())
                    {
                        CW_ENGINE_WARN("vswhere.exe returned an incomplete Visual Studio catalog record.");
                        continue;
                    }

                    const auto prerelease = val.FindMember("isPrerelease");
                    const bool isPrerelease = prerelease != val.MemberEnd() && prerelease->value.IsBool() && prerelease->value.GetBool();
                    const String displayVersionString = displayVersion->value.GetString();
                    const String productLineVersionString = productLineVersion->value.GetString();
                    const String name = String(displayName->value.GetString()) + " [" + displayVersionString + "]";
                    m_SupportedEditors.push_back(
                      { Path(productPath->value.GetString()), isPrerelease, name, GetCodeEditorVersion(productLineVersionString) });
                }
            }
        }

        AddLegacyVisualStudioInstallations(m_SupportedEditors);
        if (m_SupportedEditors.empty())
            CW_ENGINE_WARN("No supported Visual Studio installation was found.");
    }

    CodeEditor* VisualStudioCodeEditorFactory::Create(const Path& executablePath) const
    {
        for (const CodeEditorInstallation& install : m_SupportedEditors)
        {
            if (install.ExecutablePath == executablePath)
                return new VisualStudioCodeEditor(GetVisualStudioVersion(install.Version), executablePath);
        }
        return nullptr;
    }

} // namespace Crowny
#endif

#include "cwepch.h"

#include "Editor/Script/VisualStudioCodeEditor.h"
#include "Editor/Script/ScriptProjectGenerator.h"

#include "Crowny/Common/StringUtils.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/PlatformUtils.h"

// #pragma warning(disable: 4278)
// #import "libid:80cc9f66-e7d8-4ddd-85b6-d9e6cd0e93e2" version("8.0") lcid("0") raw_interfaces_only named_guids
// #pragma warning(default: 4278)
#include "dte80a.tlh"

#include <Windows.h>
#include <atlbase.h>

#include <rapidjson/document.h>

namespace Crowny
{
    static long GetRegistryStringValue(HKEY hKey, const WString& name, WString& value, const WString& defaultValue)
    {
        value = defaultValue;

        wchar_t strBuffer[1024];
        DWORD strBufferSize = sizeof(strBuffer);
        ULONG result = RegQueryValueExW(hKey, name.c_str(), 0, nullptr, (LPBYTE)strBuffer, &strBufferSize);
        if (result == ERROR_SUCCESS)
            value = strBuffer;
        return result;
    }

    struct VSProjectInfo
    {
        WString GUID;
        WString Name;
        Path path;
    };

    class VSMessageFilter : public IMessageFilter
    {
        DWORD __stdcall HandleInComingCall(DWORD dwCallType, HTASK htaskCaller, DWORD dwTickCount,
                                           LPINTERFACEINFO lpInterfaceInfo) override
        {
            return SERVERCALL_ISHANDLED;
        }

        DWORD __stdcall RetryRejectedCall(HTASK htaskCallee, DWORD dwTickCount, DWORD dwRejectType) override
        {
            if (dwRejectType == SERVERCALL_RETRYLATER)
                return 99;
            return -1;
        }

        DWORD __stdcall MessagePending(HTASK htaskCallee, DWORD dwTickCount, DWORD dwPendingType) override
        {
            return PENDINGMSG_WAITDEFPROCESS;
        }

        HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) override
        {
            if (riid == IID_IDropTarget || riid == IID_IUnknown)
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
        LONG m_RefCount;
    };

    class VisualStudio
    {
    public:
        static CComPtr<EnvDTE::_DTE> FindRunningInstance(const CLSID& clsID, const Path& solutionPath)
        {
            CComPtr<IRunningObjectTable> runningObjectTable = nullptr;
            if (FAILED(GetRunningObjectTable(0, &runningObjectTable)))
                return nullptr;

            CComPtr<IEnumMoniker> enumMoniker = nullptr;
            if (FAILED(runningObjectTable->EnumRunning(&enumMoniker)))
                return nullptr;

            CComPtr<IMoniker> dteMoniker = nullptr;
            if (FAILED(CreateClassMoniker(clsID, &dteMoniker)))
                return nullptr;

            WString widePath = solutionPath.wstring();
            CComBSTR bstrSolution(widePath.c_str());
            CComPtr<IMoniker> moniker;
            ULONG count = 0;
            while (enumMoniker->Next(1, &moniker, &count) == S_OK)
            {
                if (moniker->IsEqual(dteMoniker))
                {
                    CComPtr<IUnknown> curObject = nullptr;
                    HRESULT result = runningObjectTable->GetObject(moniker, &curObject);
                    moniker = nullptr;

                    if (result != S_OK)
                        continue;

                    CComPtr<EnvDTE::_DTE> dte;
                    curObject->QueryInterface(__uuidof(EnvDTE::_DTE), (void**)&dte);

                    if (dte == nullptr)
                        continue;

                    CComPtr<EnvDTE::_Solution> solution;
                    if (FAILED(dte->get_Solution(&solution)))
                        continue;

                    CComBSTR fullName;
                    if (FAILED(solution->get_FullName(&fullName)))
                        continue;

                    if (fullName == bstrSolution)
                        return dte;
                }
            }
            return nullptr;
        }

        static CComPtr<EnvDTE::_DTE> CreateInstance(const CLSID& clsID, const Path& solutionPath)
        {
            CComPtr<IUnknown> newInstance = nullptr;
            if (FAILED(
                  ::CoCreateInstance(clsID, nullptr, CLSCTX_LOCAL_SERVER, EnvDTE::IID__DTE, (LPVOID*)&newInstance)))
                return nullptr;
            CComPtr<EnvDTE::_DTE> dte;
            newInstance->QueryInterface(__uuidof(EnvDTE::_DTE), (void**)&dte);

            if (dte == nullptr)
                return nullptr;

            dte->put_UserControl(VARIANT_TRUE);

            CComPtr<EnvDTE::_Solution> solution;
            if (FAILED(dte->get_Solution(&solution)))
                return nullptr;

            WString widePath = solutionPath.wstring();
            CComBSTR bstrSolution(widePath.c_str());
            if (FAILED(solution->Open(bstrSolution)))
                return nullptr;

            uint32_t elapsed = 0;
            while (elapsed < 20000)
            {
                EnvDTE::Window* window = nullptr;
                if (SUCCEEDED(dte->get_MainWindow(&window)))
                    return dte;
                Sleep(100);
                elapsed += 100;
            }

            return nullptr;
        }

        static bool OpenFile(CComPtr<EnvDTE::_DTE> dte, const Path& filePath, uint32_t line)
        {
            CComPtr<EnvDTE::ItemOperations> itemOperations;
            if (FAILED(dte->get_ItemOperations(&itemOperations)))
                return false;

            WString widePath = filePath.wstring();

            CComBSTR bstrFilePath(widePath.c_str());
            CComBSTR bstrKind(EnvDTE::vsViewKindPrimary);
            CComPtr<EnvDTE::Window> window = nullptr;
            if (FAILED(itemOperations->OpenFile(bstrFilePath, bstrKind, &window)))
                return false;
            CComPtr<EnvDTE::Document> activeDocument;
            if (SUCCEEDED(dte->get_ActiveDocument(&activeDocument)))
            {
                CComPtr<IDispatch> selection;
                if (SUCCEEDED(activeDocument->get_Selection(&selection)))
                {
                    CComPtr<EnvDTE::TextSelection> textSelection;
                    if (selection != nullptr && SUCCEEDED(selection->QueryInterface(&textSelection)))
                    {
                        textSelection->GotoLine(line, VARIANT_TRUE);
                    }
                }
            }

            window = nullptr;
            if (SUCCEEDED(dte->get_MainWindow(&window)))
            {
                window->Activate();

                HWND hWnd;
                window->get_HWnd((LONG*)&hWnd);
                SetForegroundWindow(hWnd);
            }
            return true;
        }
    };

    VisualStudioCodeEditor::VisualStudioCodeEditor(VisualStudioVersion version, const Path& execPath,
                                                   const WString& clsID)
      : m_Version(version), m_ExecPath(execPath), m_ClsID(clsID)
    {
    }

    void VisualStudioCodeEditor::OpenFile(const Path& solutionPath, const Path& filePath, uint32_t line) const
    {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

        CLSID clsID;
        if (FAILED(CLSIDFromString(m_ClsID.c_str(), &clsID)))
        {
            CoUninitialize();
            return;
        }

        CComPtr<EnvDTE::_DTE> dte = VisualStudio::FindRunningInstance(clsID, solutionPath);
        if (dte == nullptr)
            dte = VisualStudio::CreateInstance(clsID, solutionPath);

        if (dte == nullptr)
        {
            CoUninitialize();
            return;
        }

        VSMessageFilter* newFilter = new VSMessageFilter();
        IMessageFilter* oldFilter;

        CoRegisterMessageFilter(newFilter, &oldFilter);
        EnvDTE::Window* window = nullptr;
        if (SUCCEEDED(dte->get_MainWindow(&window)))
            window->Activate();

        VisualStudio::OpenFile(dte, filePath, line);
        CoRegisterMessageFilter(oldFilter, nullptr);

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

            Path projectPath = solutionPath / (project.Name + ".csproj");

            Ref<DataStream> projectStream = FileSystem::CreateAndOpenFile(projectPath);
            projectStream->Write(projectString.c_str(), projectString.size() * sizeof(String::value_type));
            projectStream->Close();
        }

        Ref<DataStream> solutionStream = FileSystem::CreateAndOpenFile(solutionPathCopy);
        solutionStream->Write(solutionString.c_str(), solutionString.size() * sizeof(String::value_type));
        solutionStream->Close();
    }

    void VisualStudioCodeEditor::GetAvailableVersions()
    {
        using namespace rapidjson;

        String jsonResult = PlatformUtils::Exec("C:\\dev\\Crowny\\3rdparty\\vswhere\\vswhere.exe -prerelease -format json -utf8");
        Document document;
        document.Parse(jsonResult);
        CW_ENGINE_ASSERT(document.IsArray());
        for (const Value& val : document.GetArray())
        {
            bool isPrerelease = val.FindMember("isPrerelease")->value.GetBool();
            Path productPath = val.FindMember("productPath")->value.GetString();
            const String displayName = val.FindMember("displayName")->value.GetString();
            const String displayVersion =
              val.FindMember("catalog")->value.FindMember("productDisplayVersion")->value.GetString();
            const String name = displayName + "[" + displayVersion + "]";
            CW_ENGINE_INFO("Prerelease: {}, path: {}, name: {}", isPrerelease, productPath, name);
        }
    }
} // namespace Crowny
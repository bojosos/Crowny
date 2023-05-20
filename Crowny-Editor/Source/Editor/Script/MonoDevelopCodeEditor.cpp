#include "cwepch.h"

#include "Editor/Script/MonoDevelopCodeEditor.h"

#include "Crowny/Common/PlatformUtils.h"

namespace Crowny
{
#ifdef CW_WINDOWS
    static LONG GetRegistryStringValue(HKEY hKey, const WString& name, WString& value, const WString& defaultValue)
    {
        value = defaultValue;

        wchar_t strBuffer[512];
        DWORD strBufferSize = sizeof(strBuffer);
        ULONG result = RegQueryValueExW(hKey, name.c_str(), 0, nullptr, (LPBYTE)strBuffer, &strBufferSize);
        if (result == ERROR_SUCCESS)
            value = strBuffer;

        return result;
    }
#else

#endif

	void MonoDevelopCodeEditor::OpenFile(const Path& solutionPath, const Path& filePath, uint32_t line) const {
        String args =
          "--no-splash \"" + solutionPath.string() + "\" \"" + filePath.string() + ";" + std::to_string(line) + "\"";
        PlatformUtils::Exec(m_ExecPath + " " + args); // Will this close the pipe?
	}

    MonoDevelopCodeEditorFactory::MonoDevelopCodeEditorFactory()
    {
#ifdef CW_WINDOWS
        bool is64bit = true;
        WString registryKeyRoot;
        if (is64bit)
            registryKeyRoot = L"SOFTWARE\\Wow6432Node\\";
        else
            registryKeyRoot = L"SOFTWARE\\";
        WString registryKey = registryKeyRoot + L"\\Xamarin\\XamarinStudio";

        HKEY regKey;
        LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, registryKey.c_str(), 0, KEY_READ, &regKey);
        if (result != ERROR_SUCCESS)
            return;

        WString installPath;
        GetRegistryStringValue(regKey, L"Path", installPath, "");
        if (installPath.empty())
            return;
#elif CW_LINUX
#endif
    }
}
#pragma once

#include "Editor/Script/CodeEditor.h"

namespace Crowny
{
    enum class VisualStudioVersion
    {
        VS2008,
        VS2010,
        VS2012,
        VS2013,
        VS2015,
        VS2017,
        VS2019,
        VS2022
    };

    class VisualStudioCodeEditor : public CodeEditor
    {
    public:
        VisualStudioCodeEditor(VisualStudioVersion version, const Path& execPath, const WString& clsID);
        virtual ~VisualStudioCodeEditor() = default;

        virtual void OpenFile(const Path& solitionPath, const Path& filePath, uint32_t line) const override;
        virtual void Sync(const CodeSolutionData& data, const Path& solutionPath) const override;

        static void GetAvailableVersions();
    private:
        WString m_ClsID;
        Path m_ExecPath;
        VisualStudioVersion m_Version;
    };
}
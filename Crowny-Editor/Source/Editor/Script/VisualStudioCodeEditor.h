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
        VS2022,
        VS2026
    };

    class VisualStudioCodeEditor : public CodeEditor
    {
    public:
        VisualStudioCodeEditor(VisualStudioVersion version, const Path& execPath);
        virtual ~VisualStudioCodeEditor() = default;

        void OpenFile(const Path& solutionPath, const Path& filePath, uint32_t line) const override;
        bool Sync(const CodeSolutionData& data, const Path& solutionPath) const override;
        void SetEditorExecutablePath(const Path& path) override;
        void ReloadSolution(const CodeSolutionData& data, const Path& solutionPath) const override;

    private:
        Path m_ExecPath;
        VisualStudioVersion m_Version;
    };

    class VisualStudioCodeEditorFactory : public CodeEditorFactory
    {
    public:
        VisualStudioCodeEditorFactory();
        const Vector<CodeEditorInstallation>& GetAvailableEditors() const override { return m_SupportedEditors; }
        CodeEditor* Create(const Path& path) const override;

    private:
        Vector<CodeEditorInstallation> m_SupportedEditors;
    };
} // namespace Crowny
